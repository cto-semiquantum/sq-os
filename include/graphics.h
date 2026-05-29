#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "kernel.h"

#define VGA_WIDTH 320
#define VGA_HEIGHT 200
#define VGA_LIMIT (VGA_WIDTH * VGA_HEIGHT)
#define VGA_FRAMEBUFFER_ADDR 0xA0000
#define BACKBUFFER_ADDR      0x50000

extern uint8_t *draw_buffer;

void clear_backbuffer(void);
void draw_pixel(int x, int y, uint8_t color);
void draw_rect(int x, int y, int width, int height, uint8_t color);
void draw_char(int x, int y, char c, uint8_t color);
void draw_text(int x, int y, const char *str, uint8_t color);
void draw_char_clipped(int x, int y, char c, uint8_t color, int min_x, int max_x, int min_y, int max_y);
void draw_text_clipped(int x, int y, const char *str, uint8_t color, int min_x, int max_x, int min_y, int max_y);
void swap_buffers(void);

#endif // GRAPHICS_H
