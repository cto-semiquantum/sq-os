#include "snake.h"
#include "graphics.h"
#include "kernel.h"

extern volatile uint32_t system_ticks;

#define BOARD_W 20
#define BOARD_H 10
#define TILE_SIZE 8

typedef struct {
    int x;
    int y;
} SnakePoint;

static SnakePoint snake_body[BOARD_W * BOARD_H];
static int snake_len = 0;
static int snake_dir = 1; // 0=up, 1=right, 2=down, 3=left
static int food_x = 5;
static int food_y = 5;
static int game_over = 0;
static uint32_t last_tick = 0;

static uint32_t snake_seed = 12345;
static int snake_rand() {
    snake_seed = snake_seed * 1103515245 + 12345;
    return (unsigned int)(snake_seed / 65536) % 32768;
}

void snake_init(void) {
    snake_len = 3;
    snake_dir = 1;
    for(int i=0; i<snake_len; i++) {
        snake_body[i].x = 10 - i;
        snake_body[i].y = 5;
    }
    food_x = snake_rand() % BOARD_W;
    food_y = snake_rand() % BOARD_H;
    game_over = 0;
    last_tick = system_ticks;
}

void snake_handle_key(uint8_t scancode) {
    if (scancode == 0x48 && snake_dir != 2) snake_dir = 0; // Up
    else if (scancode == 0x4D && snake_dir != 3) snake_dir = 1; // Right
    else if (scancode == 0x50 && snake_dir != 0) snake_dir = 2; // Down
    else if (scancode == 0x4B && snake_dir != 1) snake_dir = 3; // Left
    else if (scancode == 0x39) snake_init(); // Space to restart
}

void draw_snake_content(Window *win) {
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 0); // Black

    if (game_over) {
        draw_text(win->x + 40, win->y + 50, "GAME OVER", 12);
        draw_text(win->x + 20, win->y + 70, "PRESS SPACE", 15);
        return;
    }

    if (system_ticks - last_tick > 3) {
        last_tick = system_ticks;

        for (int i = snake_len - 1; i > 0; i--) {
            snake_body[i] = snake_body[i-1];
        }

        if (snake_dir == 0) snake_body[0].y--;
        else if (snake_dir == 1) snake_body[0].x++;
        else if (snake_dir == 2) snake_body[0].y++;
        else if (snake_dir == 3) snake_body[0].x--;

        if (snake_body[0].x < 0 || snake_body[0].x >= BOARD_W ||
            snake_body[0].y < 0 || snake_body[0].y >= BOARD_H) {
            game_over = 1;
        }

        for (int i = 1; i < snake_len; i++) {
            if (snake_body[0].x == snake_body[i].x && snake_body[0].y == snake_body[i].y) {
                game_over = 1;
            }
        }

        if (snake_body[0].x == food_x && snake_body[0].y == food_y) {
            if (snake_len < BOARD_W * BOARD_H) {
                snake_len++;
            }
            food_x = snake_rand() % BOARD_W;
            food_y = snake_rand() % BOARD_H;
        }
    }

    int ox = win->x + ((win->w - (BOARD_W * TILE_SIZE)) / 2);
    int oy = win->y + 24;

    for (int i = 0; i < snake_len; i++) {
        uint8_t color = (i == 0) ? 10 : 2;
        draw_rect(ox + snake_body[i].x * TILE_SIZE, oy + snake_body[i].y * TILE_SIZE, TILE_SIZE - 1, TILE_SIZE - 1, color);
    }

    draw_rect(ox + food_x * TILE_SIZE, oy + food_y * TILE_SIZE, TILE_SIZE - 1, TILE_SIZE - 1, 12);
}
