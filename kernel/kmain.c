// language: C, file: kernel/kmain.c, target: x86_64, freestanding
#include "input.h"
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

static u32 *FB;
static u32 FB_W, FB_H, FB_PITCH;

static inline void put_px(u32 x, u32 y, u32 c){
    if (x >= FB_W || y >= FB_H) return;
    FB[y*(FB_PITCH/4)+x] = c;
}
static void fill_rect(u32 x, u32 y, u32 w, u32 h, u32 c){
    for (u32 j=0;j<h;j++) for (u32 i=0;i<w;i++) put_px(x+i, y+j, c);
}
static void blend_rect(u32 x, u32 y, u32 w, u32 h, u32 c, u8 alpha){
    for (u32 j=0;j<h;j++) for (u32 i=0;i<w;i++){
        if (x+i >= FB_W || y+j >= FB_H) continue;
        u32 *p = &FB[(y+j)*(FB_PITCH/4)+(x+i)];
        u32 old = *p;
        u32 r = (((c>>16)&0xFF)*alpha + ((old>>16)&0xFF)*(255-alpha))/255;
        u32 g = (((c>>8)&0xFF)*alpha  + ((old>>8)&0xFF)*(255-alpha))/255;
        u32 b = ((c&0xFF)*alpha        + (old&0xFF)*(255-alpha))/255;
        *p = (r<<16)|(g<<8)|b;
    }
}

static int cur_x=200, cur_y=200;

static void draw_cursor(void){
    for (int j=0;j<16;j++) for (int i=0;i<8;i++)
        if (i+j < 8) put_px(cur_x+i, cur_y+j, 0xFFFFFF);
}

static void draw_taskbar(void){
    fill_rect(0, FB_H-40, FB_W, 40, 0x1E1E1E);
    blend_rect(0, FB_H-40, FB_W, 40, 0x000000, 40);
    blend_rect(10, FB_H-32, 120, 24, 0x0078D4, 200);
    /* полупрозрачная кнопка активации внизу справа */
    blend_rect(FB_W-220, FB_H-32, 200, 24, 0x005A9E, 160);
}

static void draw_window(u32 x, u32 y, u32 w, u32 h, const char *title){
    fill_rect(x+4, y+4, w, h, 0x000000);   /* тень */
    fill_rect(x, y, w, h, 0x2D2D30);       /* тело */
    fill_rect(x, y, w, 32, 0x3E3E42);      /* заголовок */
    /* шестерёнка справа */
    u32 gx=x+w-60, gy=y+8;
    fill_rect(gx, gy, 16, 16, 0xFFFFFF);
    fill_rect(gx-2, gy+6, 20, 4, 0x3E3E42);
    fill_rect(gx+6, gy-2, 4, 20, 0x3E3E42);
    (void)title;
}

void kmain(u32 *fb, u32 w, u32 h, u32 pitch){
    FB = fb; FB_W = w; FB_H = h; FB_PITCH = pitch;

    input_init();

    for (;;){
        fill_rect(0, 0, FB_W, FB_H, 0x1A1A1A);
        draw_taskbar();
        draw_window(200, 120, 640, 420, "ANON");
        draw_window(60,  60,  300, 200, "Settings");

        int mx, my; u8 btn;
        mouse_state(&mx, &my, &btn);
        cur_x = mx; cur_y = my;
        draw_cursor();

        __asm__ volatile("hlt");
    }
}
