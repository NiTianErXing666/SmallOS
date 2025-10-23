#include "fb_console.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

/* ---------- 小工具 ---------- */
static void *k_memmove(void *d, const void *s, size_t n){
    unsigned char *D=d; const unsigned char *S=s;
    if (D==S || n==0) return d;
    if (D<S) for(size_t i=0;i<n;i++) D[i]=S[i];
    else for(size_t i=n;i>0;i--) D[i-1]=S[i-1];
    return d;
}

/* 将 8bit 通道缩放到 mask_size 位 */
static inline uint32_t scale_to_bits(uint8_t v, uint8_t bits){
    if (bits >= 8) return (uint32_t)v << (bits - 8);
    return (uint32_t)(v >> (8 - bits));
}

/* 按 framebuffer 的 mask/shift 打包像素（最关键） */
static inline uint32_t pack_rgb(struct limine_framebuffer *fb, uint32_t rgb){
    uint8_t r8=(rgb>>16)&0xFF, g8=(rgb>>8)&0xFF, b8=rgb&0xFF;
    uint32_t pr = (scale_to_bits(r8, fb->red_mask_size  ) & ((1u<<fb->red_mask_size  )-1)) << fb->red_mask_shift;
    uint32_t pg = (scale_to_bits(g8, fb->green_mask_size) & ((1u<<fb->green_mask_size)-1)) << fb->green_mask_shift;
    uint32_t pb = (scale_to_bits(b8, fb->blue_mask_size ) & ((1u<<fb->blue_mask_size )-1)) << fb->blue_mask_shift;
    return pr | pg | pb;
}

static inline void putpx(struct limine_framebuffer *fb, int x, int y, uint32_t pix){
    if (x<0||y<0||(uint32_t)x>=fb->width||(uint32_t)y>=fb->height) return;
    uint8_t *base = (uint8_t*)fb->address;
    uint32_t bppB = fb->bpp/8, pitch = fb->pitch;
    uint8_t *p = base + (uint32_t)y*pitch + (uint32_t)x*bppB;
    if (fb->bpp==32)      *(uint32_t*)p = pix;
    else if (fb->bpp==24) { p[0]=pix; p[1]=pix>>8; p[2]=pix>>16; }
    else if (fb->bpp==16) *(uint16_t*)p = (uint16_t)pix;
    else                  *(uint32_t*)p = pix; // 兜底
}

static void fill_rect(struct limine_framebuffer *fb,int x,int y,int w,int h,uint32_t pix){
    if (w<=0||h<=0) return;
    if (x<0){w-= -x; x=0;} if (y<0){h-= -y; y=0;}
    if (x+w>(int)fb->width)  w=(int)fb->width -x;
    if (y+h>(int)fb->height) h=(int)fb->height-y;
    uint8_t *base=(uint8_t*)fb->address; uint32_t pitch=fb->pitch;
    if (fb->bpp==32){
        for(int j=0;j<h;j++){ uint32_t *row=(uint32_t*)(base + (uint32_t)(y+j)*pitch + (uint32_t)x*4);
            for(int i=0;i<w;i++) row[i]=pix; }
    }else if (fb->bpp==24){
        for(int j=0;j<h;j++){ uint8_t *row=base + (uint32_t)(y+j)*pitch + (uint32_t)x*3;
            for(int i=0;i<w;i++){ row[i*3+0]=pix; row[i*3+1]=pix>>8; row[i*3+2]=pix>>16; } }
    }else if (fb->bpp==16){
        for(int j=0;j<h;j++){ uint16_t *row=(uint16_t*)(base + (uint32_t)(y+j)*pitch + (uint32_t)x*2);
            for(int i=0;i<w;i++) row[i]=(uint16_t)pix; }
    }else{
        for(int j=0;j<h;j++){ uint32_t *row=(uint32_t*)(base + (uint32_t)(y+j)*pitch + (uint32_t)x*4);
            for(int i=0;i<w;i++) row[i]=pix; }
    }
}

/* ---------- 极简 5x7 字体（ASCII 常用） ---------- */
typedef struct { char ch; uint8_t rows[7]; } glyph5x7_t;
#define G(_c,_0,_1,_2,_3,_4,_5,_6) (glyph5x7_t){_c,{_0,_1,_2,_3,_4,_5,_6}}

static const glyph5x7_t GLYPHS[] = {
    /* 空格与常用符号 */
    G(' ',0,0,0,0,0,0,0),  G('.',0,0,0,0,0,0x04,0),
    G(':',0,0x04,0,0,0x04,0,0), G('-',0,0,0x1F,0,0,0,0),
    G('_',0,0,0,0,0,0x1F,0), G('!',0x04,0x04,0x04,0x04,0,0x04,0),
    /* 数字 0-9 */
    G('0',0x0E,0x11,0x13,0x15,0x19,0x11,0x0E),
    G('1',0x04,0x0C,0x04,0x04,0x04,0x04,0x0E),
    G('2',0x0E,0x11,0x01,0x02,0x04,0x08,0x1F),
    G('3',0x1F,0x02,0x04,0x02,0x01,0x11,0x0E),
    G('4',0x02,0x06,0x0A,0x12,0x1F,0x02,0x02),
    G('5',0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E),
    G('6',0x06,0x08,0x10,0x1E,0x11,0x11,0x0E),
    G('7',0x1F,0x01,0x02,0x04,0x08,0x08,0x08),
    G('8',0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E),
    G('9',0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C),
    /* 大写 A-Z */
    G('A',0x0E,0x11,0x11,0x1F,0x11,0x11,0x11),
    G('B',0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E),
    G('C',0x0E,0x11,0x10,0x10,0x10,0x11,0x0E),
    G('D',0x1E,0x11,0x11,0x11,0x11,0x11,0x1E),
    G('E',0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F),
    G('F',0x1F,0x10,0x10,0x1E,0x10,0x10,0x10),
    G('G',0x0E,0x11,0x10,0x17,0x11,0x11,0x0E),
    G('H',0x11,0x11,0x11,0x1F,0x11,0x11,0x11),
    G('I',0x0E,0x04,0x04,0x04,0x04,0x04,0x0E),
    G('J',0x1F,0x01,0x01,0x01,0x11,0x11,0x0E),
    G('K',0x11,0x12,0x14,0x18,0x14,0x12,0x11),
    G('L',0x10,0x10,0x10,0x10,0x10,0x10,0x1F),
    G('M',0x11,0x1B,0x15,0x11,0x11,0x11,0x11),
    G('N',0x11,0x19,0x15,0x13,0x11,0x11,0x11),
    G('O',0x0E,0x11,0x11,0x11,0x11,0x11,0x0E),
    G('P',0x1E,0x11,0x11,0x1E,0x10,0x10,0x10),
    G('Q',0x0E,0x11,0x11,0x11,0x15,0x12,0x0D),
    G('R',0x1E,0x11,0x11,0x1E,0x14,0x12,0x11),
    G('S',0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E),
    G('T',0x1F,0x04,0x04,0x04,0x04,0x04,0x04),
    G('U',0x11,0x11,0x11,0x11,0x11,0x11,0x0E),
    G('V',0x11,0x11,0x11,0x11,0x11,0x0A,0x04),
    G('W',0x11,0x11,0x11,0x15,0x15,0x1B,0x11),
    G('X',0x11,0x11,0x0A,0x04,0x0A,0x11,0x11),
    G('Y',0x11,0x11,0x0A,0x04,0x04,0x04,0x04),
    G('Z',0x1F,0x01,0x02,0x04,0x08,0x10,0x1F),
};

static const glyph5x7_t* find_glyph(char c){
    if (c>='a'&&c<='z') c-=32;
    for (size_t i=0;i<sizeof(GLYPHS)/sizeof(GLYPHS[0]);++i)
        if (GLYPHS[i].ch==c) return &GLYPHS[i];
    return &GLYPHS[0]; // 空格
}

static void draw_glyph(struct limine_framebuffer *fb,int x,int y,const glyph5x7_t *g,int s,uint32_t pix){
    for(int r=0;r<7;r++){
        uint8_t bits=g->rows[r];
        for(int c=0;c<5;c++){
            if (bits & (1u<<(4-c))) fill_rect(fb, x+c*s, y+r*s, s, s, pix);
        }
    }
}

/* ---------- 控制台实现 ---------- */
static void scroll_up(fbcon_t *con){
    struct limine_framebuffer *fb=con->fb;
    int line = (con->gh + con->gap) * con->scale;
    uint8_t *base=(uint8_t*)fb->address; uint32_t pitch=fb->pitch;
    size_t bytes = (fb->height - line) * (size_t)pitch;
    k_memmove(base, base + (size_t)line * pitch, bytes);
    fill_rect(fb, 0, (int)fb->height - line, (int)fb->width, line, pack_rgb(fb, con->bg_rgb));
    if (con->cy >= line) con->cy -= line; else con->cy = 0;
}

void fbcon_init(fbcon_t *con, struct limine_framebuffer *fb,
                uint32_t fg_rgb, uint32_t bg_rgb, int scale){
    con->fb=fb; con->fg_rgb=fg_rgb; con->bg_rgb=bg_rgb;
    con->scale = (scale<1)?1:scale;
    con->gw=5; con->gh=7; con->gap=1;
    con->cx=0; con->cy=0;
    fbcon_clear(con);
}

void fbcon_clear(fbcon_t *con){
    fill_rect(con->fb, 0, 0, (int)con->fb->width, (int)con->fb->height,
              pack_rgb(con->fb, con->bg_rgb));
    con->cx=con->cy=0;
}
void fbcon_set_color(fbcon_t *con, uint32_t fg_rgb, uint32_t bg_rgb){ con->fg_rgb=fg_rgb; con->bg_rgb=bg_rgb; }

static void newline(fbcon_t *con){
    con->cx = 0;
    con->cy += (con->gh + con->gap) * con->scale;
    if ((uint32_t)(con->cy + con->gh*con->scale) > con->fb->height) scroll_up(con);
}

void fbcon_putc(fbcon_t *con, char ch){
    if (ch=='\n'){ newline(con); return; }
    if (ch=='\r'){ con->cx=0; return; }
    if (ch=='\t'){ con->cx += (con->gw + con->gap) * con->scale * 4; return; }

    int s=con->scale, cw=(con->gw+con->gap)*s;
    if ((uint32_t)(con->cx + con->gw*s) > con->fb->width) newline(con);
    if ((uint32_t)(con->cy + con->gh*s) > con->fb->height) scroll_up(con);

    draw_glyph(con->fb, con->cx, con->cy, find_glyph(ch), s, pack_rgb(con->fb, con->fg_rgb));
    con->cx += cw;
}
void fbcon_write(fbcon_t *con, const char *s){ while(*s) fbcon_putc(con, *s++); }

/* 简易 printf：%s %d %u %x %p %% */
static void itoa_u(char *buf, unsigned long long v, unsigned base, bool lower){
    static const char *D="0123456789ABCDEF", *d="0123456789abcdef";
    char tmp[32]; int i=0; if (!v){buf[0]='0';buf[1]=0;return;}
    while(v && i<(int)sizeof(tmp)){ unsigned r=v%base; tmp[i++]= lower?d[r]:D[r]; v/=base; }
    int p=0; while(i--) buf[p++]=tmp[i]; buf[p]=0;
}
void fbcon_printf(fbcon_t *con, const char *fmt, ...){
    va_list ap; va_start(ap, fmt); char num[64];
    for (const char *p=fmt; *p; ++p){
        if (*p!='%'){ fbcon_putc(con,*p); continue; }
        switch (*++p){
            case '%': fbcon_putc(con,'%'); break;
            case 's': { const char *s=va_arg(ap,const char*); fbcon_write(con, s?s:"(null)"); } break;
            case 'd': { long long v=va_arg(ap,int); if (v<0){ fbcon_putc(con,'-'); v=-v; }
                       itoa_u(num,(unsigned long long)v,10,false); fbcon_write(con,num);} break;
            case 'u': { unsigned int v=va_arg(ap,unsigned int);
                       itoa_u(num,v,10,false); fbcon_write(con,num);} break;
            case 'x': { unsigned int v=va_arg(ap,unsigned int);
                       fbcon_write(con,"0x"); itoa_u(num,v,16,true); fbcon_write(con,num);} break;
            case 'p': { uintptr_t v=(uintptr_t)va_arg(ap, void*);
                       fbcon_write(con,"0x"); itoa_u(num,(unsigned long long)v,16,true); fbcon_write(con,num);} break;
            default: fbcon_putc(con,'?');
        }
    }
    va_end(ap);
}
