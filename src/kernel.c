// src/kernel.c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

/* 要求 Limine 协议基线修订 */
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

/* === 关键：请求帧缓冲（代替 Terminal） === */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

/* 请求段标记（必须存在） */
__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

/* freestanding 基础函数 */
void *memcpy(void *d, const void *s, size_t n){unsigned char*D=d;const unsigned char*S=s;for(size_t i=0;i<n;i++)D[i]=S[i];return d;}
void *memmove(void *d,const void *s,size_t n){unsigned char*D=d;const unsigned char*S=s;if(D<S){for(size_t i=0;i<n;i++)D[i]=S[i];}else if(D>S){for(size_t i=n;i>0;i--)D[i-1]=S[i-1];}return d;}
void *memset(void *s,int c,size_t n){unsigned char*p=s;for(size_t i=0;i<n;i++)p[i]=(unsigned char)c;return s;}
int memcmp(const void *a,const void *b,size_t n){const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;return 0;}

/* 0xE9 调试口（配合 -debugcon stdio） */
static inline void outb(unsigned short port, unsigned char val){__asm__ __volatile__("outb %0,%1"::"a"(val),"Nd"(port));}
static void dbg_puts(const char*s){while(*s)outb(0xE9,(unsigned char)*s++);}

/* 简单忙等 */
static void delay_cycles(volatile unsigned long long c){while(c--)__asm__ __volatile__("pause");}

/* 像素/矩形/极简字库与绘制 */
static inline void putpx(struct limine_framebuffer*fb,int x,int y,uint32_t rgba){
    if(x<0||y<0||(uint32_t)x>=fb->width||(uint32_t)y>=fb->height)return;
    volatile uint32_t*base=(volatile uint32_t*)fb->address; uint32_t stride=fb->pitch/4;
    base[(uint32_t)y*stride+(uint32_t)x]=rgba;
}
static void rect(struct limine_framebuffer*fb,int x,int y,int w,int h,uint32_t rgba){
    for(int j=0;j<h;j++)for(int i=0;i<w;i++)putpx(fb,x+i,y+j,rgba);
}
static const uint8_t G_H[7]={0x11,0x11,0x1F,0x11,0x11,0x11,0x00};
static const uint8_t G_E[7]={0x1F,0x10,0x1E,0x10,0x10,0x1F,0x00};
static const uint8_t G_L[7]={0x10,0x10,0x10,0x10,0x10,0x1F,0x00};
static const uint8_t G_O[7]={0x0E,0x11,0x11,0x11,0x11,0x0E,0x00};
static const uint8_t G_W[7]={0x11,0x11,0x15,0x15,0x0A,0x0A,0x00};
static const uint8_t G_R[7]={0x1E,0x11,0x1E,0x14,0x12,0x11,0x00};
static const uint8_t G_D[7]={0x1E,0x11,0x11,0x11,0x11,0x1E,0x00};
static const uint8_t G_S[7]={0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t* pick(char c){
    switch(c){case 'H':return G_H;case 'E':return G_E;case 'L':return G_L;case 'O':return G_O;
               case 'W':return G_W;case 'R':return G_R;case 'D':return G_D;case ' ':return G_S;
               default:return G_S;}
}
static void draw_char(struct limine_framebuffer*fb,int x,int y,char c,int s,uint32_t color){
    const uint8_t*g=pick(c);
    for(int r=0;r<7;r++){uint8_t bits=g[r];
        for(int col=0;col<5;col++) if(bits&(1u<<(4-col))) rect(fb,x+col*s,y+r*s,s,s,color);
    }
}
static void draw_str(struct limine_framebuffer*fb,int x,int y,const char*str,int s,uint32_t color){
    for(int cx=x;*str;str++,cx+=5*s+s) draw_char(fb,cx,y,*str,s,color);
}

/* 常驻自旋，防止“息屏” */
static void stay_awake(void){for(;;)__asm__ __volatile__("pause");}

void kmain(void){
    dbg_puts("entered kmain\n");
    if(!LIMINE_BASE_REVISION_SUPPORTED) stay_awake();

    if(framebuffer_request.response==NULL || framebuffer_request.response->framebuffer_count<1){
        dbg_puts("framebuffer unavailable\n");
        stay_awake();
    }

    struct limine_framebuffer*fb = framebuffer_request.response->framebuffers[0];

    rect(fb,0,0,(int)fb->width,(int)fb->height,0x202020FFu);                     // 背景
    draw_str(fb,40,40,"HELLO WORLD",4,0xFFFFFFFFu);                               // 文本

    for(;;){                                                                       // 保持活跃 & 定期重绘
        delay_cycles(500000000ULL);
        draw_str(fb,40,40,"HELLO WORLD",4,0xFFFFFFFFu);
    }
}
