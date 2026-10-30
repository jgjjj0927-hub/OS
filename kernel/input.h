// language: C, file: kernel/input.h
#pragma once
#include <stdint.h>

void     input_init(void);
uint64_t timer_ticks(void);
int      kbd_try_get(char *out);
void     mouse_state(int *x, int *y, uint8_t *btn);
