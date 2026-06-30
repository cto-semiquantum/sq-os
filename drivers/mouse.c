#include "mouse.h"

int32_t mouse_x = 160;
int32_t mouse_y = 100;
uint8_t mouse_cycle = 0;
uint8_t mouse_byte0 = 0;
uint8_t mouse_byte1 = 0;
uint8_t mouse_byte2 = 0;
uint8_t last_mouse_buttons = 0;

void init_mouse(void) {
    mouse_x = 160;
    mouse_y = 100;
    mouse_cycle = 0;
    last_mouse_buttons = 0;
}

void draw_cursor(void) {
    int cx = mouse_x;
    int cy = mouse_y;

    // Clamp so 7x7 cursor stays fully on screen
    if (cx > 313) cx = 313;
    if (cy > 193) cy = 193;

    // Draw 7x7 black outline
    draw_rect(cx, cy, 7, 7, 0);

    // Draw 5x5 white inner body
    draw_rect(cx + 1, cy + 1, 5, 5, 15);

    // Draw 1x1 black center dot
    draw_rect(cx + 3, cy + 3, 1, 1, 0);
}
