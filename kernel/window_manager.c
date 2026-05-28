#include "window_manager.h"

// Instantiate global windows
Window window_files;
Window window_terminal;
Window window_settings;
Window window_welcome;

Window *window_order[NUM_WINDOWS];

Window *dragged_window = NULL;
int32_t drag_offset_x = 0;
int32_t drag_offset_y = 0;

// Content drawing function declarations (implemented in application-specific files)
extern void draw_welcome_content(Window *win);
extern void draw_files_content(Window *win);
extern void draw_terminal_content(Window *win);
extern void draw_settings_content(Window *win);

void init_windows(void) {
    // Welcome Window (focused and visible on boot)
    window_welcome.x = 60;
    window_welcome.y = 30;
    window_welcome.w = 190;
    window_welcome.h = 95;
    window_welcome.title = "WELCOME";
    window_welcome.visible = 1;
    window_welcome.active = 1;
    window_welcome.color = 7; // Light gray

    // Files Window
    window_files.x = 20;
    window_files.y = 20;
    window_files.w = 180;
    window_files.h = 100;
    window_files.title = "FILES";
    window_files.visible = 0;
    window_files.active = 0;
    window_files.color = 7;

    // Terminal Window
    window_terminal.x = 50;
    window_terminal.y = 45;
    window_terminal.w = 200;
    window_terminal.h = 105;
    window_terminal.title = "TERMINAL";
    window_terminal.visible = 0;
    window_terminal.active = 0;
    window_terminal.color = 7;

    // Settings Window
    window_settings.x = 80;
    window_settings.y = 70;
    window_settings.w = 160;
    window_settings.h = 90;
    window_settings.title = "SETTINGS";
    window_settings.visible = 0;
    window_settings.active = 0;
    window_settings.color = 7;

    // Set initial Z-order (Welcome on top)
    window_order[0] = &window_files;
    window_order[1] = &window_terminal;
    window_order[2] = &window_settings;
    window_order[3] = &window_welcome;
}

void draw_window_border(int x, int y, int w, int h, uint8_t color) {
    // Body (light gray)
    draw_rect(x, y, w, h, color);

    // Top highlight (white)
    draw_rect(x, y, w, 1, 15);

    // Left highlight (white)
    draw_rect(x, y, 1, h, 15);

    // Bottom shadow (dark gray)
    draw_rect(x, y + h - 1, w, 1, 8);

    // Right shadow (dark gray)
    draw_rect(x + w - 1, y, 1, h, 8);
}

void draw_titlebar(int x, int y, int w, const char *title, int active) {
    uint8_t bar_color = active ? 1 : 8;        // Active: Blue (1), Inactive: Dark Gray (8)
    uint8_t text_color = active ? 15 : 7;      // Active: White (15), Inactive: Light Gray (7)

    // Draw titlebar rectangle
    draw_rect(x + 2, y + 2, w - 4, 10, bar_color);

    // Draw title string
    draw_text(x + 6, y + 3, title, text_color);
}

void draw_close_button(int x, int y, int w) {
    int btn_x = x + w - 13;
    int btn_y = y + 3;

    // Button body
    draw_rect(btn_x, btn_y, 8, 8, 7);

    // Top and Left highlight (white)
    draw_rect(btn_x, btn_y, 8, 1, 15);
    draw_rect(btn_x, btn_y, 1, 8, 15);

    // Bottom and Right shadow (dark gray)
    draw_rect(btn_x, btn_y + 7, 8, 1, 8);
    draw_rect(btn_x + 7, btn_y, 1, 8, 8);

    // X glyph (black)
    draw_char(btn_x, btn_y, 'X', 0);
}

void draw_window(Window *win) {
    if (!win->visible) return;

    // 1. Draw window border/frame
    draw_window_border(win->x, win->y, win->w, win->h, win->color);

    // 2. Draw titlebar
    draw_titlebar(win->x, win->y, win->w, win->title, win->active);

    // 3. Draw close button
    draw_close_button(win->x, win->y, win->w);

    // 4. Draw contents depending on the window
    if (win == &window_welcome) {
        draw_welcome_content(win);
    } else if (win == &window_files) {
        draw_files_content(win);
    } else if (win == &window_terminal) {
        draw_terminal_content(win);
    } else if (win == &window_settings) {
        draw_settings_content(win);
    }
}

void draw_all_windows(void) {
    for (int i = 0; i < NUM_WINDOWS; i++) {
        draw_window(window_order[i]);
    }
}

void focus_window(Window *win) {
    // Deactivate all windows
    window_files.active = 0;
    window_terminal.active = 0;
    window_settings.active = 0;
    window_welcome.active = 0;

    // Activate selected window
    win->active = 1;

    // Find its current position in Z-order array
    int idx = -1;
    for (int i = 0; i < NUM_WINDOWS; i++) {
        if (window_order[i] == win) {
            idx = i;
            break;
        }
    }

    if (idx != -1) {
        // Shift remaining windows left to fill the gap
        for (int i = idx; i < NUM_WINDOWS - 1; i++) {
            window_order[i] = window_order[i + 1];
        }
        // Place selected window at the top (end of list)
        window_order[NUM_WINDOWS - 1] = win;
    }
}
