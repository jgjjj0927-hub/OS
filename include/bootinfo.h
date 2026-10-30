/* language: C, file: include/bootinfo.h — общий контракт загрузчик↔ядро */
#pragma once
#include <stdint.h>

#define BOOTINFO_MAGIC 0x4D41484AUL

typedef struct {
    uint64_t magic;          /* BOOTINFO_MAGIC */
    uint64_t fb_base;        /* физический адрес фреймбуфера */
    uint64_t fb_size;        /* размер в байтах */
    uint32_t width;          /* пикселей */
    uint32_t height;         /* пикселей */
    uint32_t pitch_px;       /* пикселей на скан-линию */
    uint32_t bpp;            /* бит на пиксель, всегда 32 в этой сборке */
    uint32_t pixel_format;   /* 0 = RGB, 1 = BGR */
    uint32_t _pad;
} BootInfo;
