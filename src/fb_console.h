#pragma once
#include <stdint.h>
#include "limine.h"

typedef struct {
    struct limine_framebuffer *fb;
    uint32_t fg_rgb, bg_rgb;
    int scale, cx, cy;    // 光标像素位置
    int gw, gh, gap;      // 字形宽高与字间距（逻辑像素）
} fbcon_t;

void fbcon_init (fbcon_t *con, struct limine_framebuffer *fb,
                 uint32_t fg_rgb, uint32_t bg_rgb, int scale);
void fbcon_clear(fbcon_t *con);
void fbcon_set_color(fbcon_t *con, uint32_t fg_rgb, uint32_t bg_rgb);
void fbcon_putc (fbcon_t *con, char c);
void fbcon_write(fbcon_t *con, const char *s);
void fbcon_printf(fbcon_t *con, const char *fmt, ...);   // 支持 %s %d %u %x %p %%
