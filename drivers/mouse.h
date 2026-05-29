#ifndef MOUSE_H
#define MOUSE_H

#include "graphics.h"

extern int32_t mouse_x;
extern int32_t mouse_y;
extern uint8_t mouse_cycle;
extern uint8_t mouse_byte0;
extern uint8_t mouse_byte1;
extern uint8_t mouse_byte2;
extern uint8_t last_mouse_buttons;

void draw_cursor(void);
void init_mouse(void);

#endif // MOUSE_H
