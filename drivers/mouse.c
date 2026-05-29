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

    // Clamp so 5x5 cursor stays fully on screen
    if (cx > 315) cx = 315;
    if (cy > 195) cy = 195;

    // 5x5 white square (color 15)
    draw_rect(cx, cy, 5, 5, 15);

    // 1x1 black center dot at mouse_x + 2, mouse_y + 2 (color 0)
    // Make sure we clamp the inner dot too
    int dx = mouse_x + 2;
    int dy = mouse_y + 2;
    if (dx > 317) dx = 317;
    if (dy > 197) dy = 197;
    draw_rect(dx, dy, 1, 1, 0);
}
