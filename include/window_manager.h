#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include "graphics.h"

typedef struct Window {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    const char *title;
    int32_t visible;
    int32_t active;
    uint8_t color;
} Window;

#define NUM_WINDOWS 4
extern Window window_files;
extern Window window_terminal;
extern Window window_settings;
extern Window window_welcome;

extern Window *window_order[NUM_WINDOWS];

extern Window *dragged_window;
extern int32_t drag_offset_x;
extern int32_t drag_offset_y;

void draw_window_border(int x, int y, int w, int h, uint8_t color);
void draw_titlebar(int x, int y, int w, const char *title, int active);
void draw_close_button(int x, int y, int w);
void draw_window(Window *win);
void draw_all_windows(void);
void focus_window(Window *win);
void init_windows(void);

#endif // WINDOW_MANAGER_H
