#ifndef SNAKE_H
#define SNAKE_H

#include <stdint.h>
#include "window_manager.h"

void snake_init(void);
void snake_handle_key(uint8_t scancode);
void draw_snake_content(Window *win);

#endif
