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
/* --- 2) 我们要的几个请求：帧缓冲 + 内存映射 + 引导器信息 + HHDM ------- */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST, .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0
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

  /* 其它信息：内存映射、引导器、HHDM */
    if(memmap_request.response){
        dbg_puts("memmap get!\n"); 
    }
    if(bootloader_info_request.response){
        dbg_puts("bootloader get! \n");
    }
    if(hhdm_request.response){
        dbg_puts("hhdm get！ \n");
    }

    fbcon_t con;
    fbcon_init(&con, fb, 0xFFFFFF, 0x202020, 2); // 白字、深灰底、4倍放大

    fbcon_write(&con, "HELLO WORLD \nthis is by Calvin\n");
    fbcon_printf(&con, "res=%ux%u bpp=%u pitch=%u\n",
                 fb->width, fb->height, fb->bpp, fb->pitch);
    fbcon_printf(&con, "fb=0x%p\n", fb->address);
    fbcon_printf(&con, "memmap entries=0x%p\n", memmap_request.response->entries);
    fbcon_printf(&con, "memmap entry_count=0x%p\n", memmap_request.response->entry_count);

    fbcon_printf(&con, "hhdm offset: 0x%p\n", hhdm_request.response->offset);



    /* 常驻：每隔一会儿再打一行，窗口一直有更新 */
    unsigned counter = 0;
    for (;;) {
        delay_cycles(200000000ULL);
        fbcon_printf(&con, "tick %u\n", counter++);
    }
}
