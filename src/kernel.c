// src/kernel.c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"
#include "fb_console.h"

/* Limine 基线修订 */
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

/* 请求帧缓冲 */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

/* 请求段标记 */
__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

/* 0xE9 调试口（配合 -debugcon stdio） */
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__("outb %0,%1" : : "a"(val), "Nd"(port));
}
static void dbg_puts(const char *s) { while (*s) outb(0xE9, (unsigned char)*s++); }

/* 简单延时，保持活跃避免“息屏” */
static void delay_cycles(volatile unsigned long long c) {
    while (c--) __asm__ __volatile__("pause");
}

void kmain(void) {
    dbg_puts("entered kmain\n");
    if (!LIMINE_BASE_REVISION_SUPPORTED) for(;;)__asm__ __volatile__("pause");

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        dbg_puts("framebuffer unavailable\n");
        for(;;)__asm__ __volatile__("pause");
    }

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    fbcon_t con;
    fbcon_init(&con, fb, 0xFFFFFF, 0x202020, 2); // 白字、深灰底、4倍放大

    fbcon_write(&con, "HELLO WORLD \nthis is by Calvin\n");
    fbcon_printf(&con, "res=%ux%u bpp=%u pitch=%u\n",
                 fb->width, fb->height, fb->bpp, fb->pitch);
    fbcon_printf(&con, "fb=0x%p\n", fb->address);

    /* 常驻：每隔一会儿再打一行，窗口一直有更新 */
    unsigned counter = 0;
    for (;;) {
        delay_cycles(200000000ULL);
        fbcon_printf(&con, "tick %u\n", counter++);
    }
}
