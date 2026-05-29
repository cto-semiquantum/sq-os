#include "desktop.h"
#include "terminal_app.h"
#include "rtc.h"
#include "wallpaper.h"

static void draw_icon_box(int x, int y, uint8_t color) {
    draw_rect(x, y, 10, 10, color);
    draw_rect(x, y, 10, 1, 15);      // White highlight (top)
    draw_rect(x, y, 1, 10, 15);      // White highlight (left)
    draw_rect(x, y + 9, 10, 1, 8);   // Dark gray shadow (bottom)
    draw_rect(x + 9, y, 1, 10, 8);   // Dark gray shadow (right)
}

// Content Drawers
void draw_welcome_content(Window *win) {
    // White body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 15);
    // Dark gray separator line
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // Text lines
    draw_text(win->x + 10, win->y + 22, "Welcome to SQ-OS!", 0);
    draw_text(win->x + 10, win->y + 38, "Architecture: 32-bit PM", 0);
    draw_text(win->x + 10, win->y + 54, "Status: GUI Active", 1); // Blue text
}

void draw_files_content(Window *win) {
    // White body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 24, 15);
    // Separator line
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // File rows
    // Row 1
    draw_rect(win->x + 10, win->y + 22, 6, 6, 1);
    draw_text(win->x + 20, win->y + 21, "kernel.asm", 0);

    // Row 2
    draw_rect(win->x + 10, win->y + 34, 6, 6, 1);
    draw_text(win->x + 20, win->y + 33, "boot.asm", 0);

    // Row 3
    draw_rect(win->x + 10, win->y + 46, 6, 6, 1);
    draw_text(win->x + 20, win->y + 45, "readme.md", 0);

    // Row 4
    draw_rect(win->x + 10, win->y + 58, 6, 6, 1);
    draw_text(win->x + 20, win->y + 57, "system.cfg", 0);

    // Status bar (gray background)
    draw_rect(win->x + 4, win->y + win->h - 9, win->w - 8, 8, 7);
    draw_text(win->x + 8, win->y + win->h - 9, "4 items", 8);
}

void draw_settings_content(Window *win) {
    // Light gray body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 7);

    // Info labels
    draw_text(win->x + 10, win->y + 22, "Res: 320x200", 0);
    draw_text(win->x + 10, win->y + 36, "Colors: 256", 0);
    draw_text(win->x + 10, win->y + 50, "Mouse: OK", 0);

    // Mock "Save" button background
    int btn_x = win->x + 10;
    int btn_y = win->y + 66;
    draw_rect(btn_x, btn_y, 40, 12, 7);

    // Button highlight (white)
    draw_rect(btn_x, btn_y, 40, 1, 15);
    draw_rect(btn_x, btn_y, 1, 12, 15);

    // Button shadow (dark gray)
    draw_rect(btn_x, btn_y + 11, 40, 1, 8);
    draw_rect(btn_x + 39, btn_y, 1, 12, 8);

    // Button text
    draw_text(btn_x + 8, btn_y + 2, "Save", 0);
}

// Redraw entire desktop
void redraw_desktop(void) {
    // 1. Clear backbuffer to black
    clear_backbuffer();

    // 2. Wallpaper background (retro gradient — covers full 320x200)
    render_wallpaper();

    // 3. Top bar (cyan, Y=0..9)
    draw_rect(0, 0, 320, 10, 3);
    draw_text(100, 1, "SQ-OS DESKTOP v2.0", 15);

    // 4. Taskbar (retro light gray, Y=190..199)
    draw_rect(0, 190, 320, 10, 7);
    draw_rect(0, 190, 320, 1, 15); // Bevel top highlight line

    // Fetch and cache time from RTC (approx once per second to prevent CMOS bus spamming)
    static int rtc_tick = 0;
    static char cached_time_str[6] = "00:00";
    rtc_tick++;
    if (rtc_tick >= 60 || rtc_tick == 1) {
        rtc_tick = 1;
        int hours = 0, minutes = 0;
        read_rtc(&hours, &minutes);
        cached_time_str[0] = '0' + (hours / 10);
        cached_time_str[1] = '0' + (hours % 10);
        cached_time_str[2] = ':';
        cached_time_str[3] = '0' + (minutes / 10);
        cached_time_str[4] = '0' + (minutes % 10);
        cached_time_str[5] = '\0';
    }

    draw_text(200, 191, "ESC=SHELL", 0);       // Black text
    draw_text(275, 191, cached_time_str, 0);   // Black clock text

    // 5. Desktop icons with 3D bevel frames
    // Icon: SETTINGS (yellow, X=15 Y=25)
    draw_icon_box(15, 25, 14);
    draw_text(6, 38, "SETTINGS", 15);

    // Icon: FILES (green, X=15 Y=65)
    draw_icon_box(15, 65, 10);
    draw_text(12, 78, "FILES", 15);

    // Icon: TERMINAL (light red, X=15 Y=105)
    draw_icon_box(15, 105, 12);
    draw_text(6, 118, "TERMINAL", 15);

    // 6. Draw all windows in Z-order
    draw_all_windows();

    // 7. Draw cursor on top
    draw_cursor();

    // 8. Swap buffers (copies to screen with Vsync wait)
    swap_buffers();
}

// Mouse click handler
void handle_click_event(int cx, int cy) {
    // Check windows from top to bottom (reverse order: index 3 to 0)
    for (int i = NUM_WINDOWS - 1; i >= 0; i--) {
        Window *win = window_order[i];

        if (!win->visible) continue;

        // Check if mouse is inside window bounds
        if (cx >= win->x && cx < win->x + win->w &&
            cy >= win->y && cy < win->y + win->h) {

            // Focus this window
            focus_window(win);

            // Check if close button is clicked
            int btn_x = win->x + win->w - 13;
            int btn_y = win->y + 3;
            if (cx >= btn_x && cx < btn_x + 8 &&
                cy >= btn_y && cy < btn_y + 8) {
                
                win->visible = 0;
                win->active = 0;

                // Focus next visible window
                for (int j = NUM_WINDOWS - 1; j >= 0; j--) {
                    if (window_order[j]->visible) {
                        focus_window(window_order[j]);
                        break;
                    }
                }
                return;
            }

            // Check if titlebar is clicked
            if (cx >= win->x && cx < win->x + win->w - 14 &&
                cy >= win->y && cy < win->y + 12) {
                
                dragged_window = win;
                drag_offset_x = cx - win->x;
                drag_offset_y = cy - win->y;
            }
            return; // Don't check anything else
        }
    }

    // Check desktop icons if no window was clicked
    // SETTINGS icon: X [5..45], Y [20..50]
    if (cx >= 5 && cx <= 45 && cy >= 20 && cy <= 50) {
        window_settings.visible = 1;
        focus_window(&window_settings);
        return;
    }

    // FILES icon: X [5..45], Y [60..90]
    if (cx >= 5 && cx <= 45 && cy >= 60 && cy <= 90) {
        window_files.visible = 1;
        focus_window(&window_files);
        return;
    }

    // TERMINAL icon: X [5..45], Y [100..130]
    if (cx >= 5 && cx <= 45 && cy >= 100 && cy <= 130) {
        window_terminal.visible = 1;
        focus_window(&window_terminal);
        return;
    }
}

void run_gui_loop(void) {
    // Reset window instances and coordinates
    init_windows();
    init_mouse();
    init_terminal_app();

    while (1) {
        // Drain keyboard and mouse events non-blockingly
        while (inb(0x64) & 0x01) {
            uint8_t status = inb(0x64);
            if (status & 0x20) { // Mouse data
                uint8_t data = inb(0x60);
                if (mouse_cycle == 0) {
                    if (data & 0x08) { // Sync bit check
                        mouse_byte0 = data;
                        mouse_cycle = 1;
                    }
                } else if (mouse_cycle == 1) {
                    mouse_byte1 = data;
                    mouse_cycle = 2;
                } else if (mouse_cycle == 2) {
                    mouse_byte2 = data;
                    mouse_cycle = 0;

                    // Parse full packet
                    uint8_t buttons = mouse_byte0;
                    int32_t dx = (int32_t)mouse_byte1;
                    int32_t dy = (int32_t)mouse_byte2;

                    if (mouse_byte0 & 0x10) { // X sign
                        dx |= 0xFFFFFF00;
                    }
                    if (mouse_byte0 & 0x20) { // Y sign
                        dy |= 0xFFFFFF00;
                    }

                    mouse_x += dx;
                    mouse_y -= dy; // Inverted relative to screen Y

                    // Clamp cursor coordinates
                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_x > 319) mouse_x = 319;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_y > 199) mouse_y = 199;

                    // Handle left click and dragging
                    if (buttons & 1) {
                        if (!(last_mouse_buttons & 1)) {
                            // Fresh click
                            handle_click_event(mouse_x, mouse_y);
                        } else if (dragged_window) {
                            // Mouse hold + drag window
                            int new_x = mouse_x - drag_offset_x;
                            int new_y = mouse_y - drag_offset_y;

                            // Clamp titlebar within screen bounds
                            if (new_x < -100) new_x = -100;
                            if (new_x > 280) new_x = 280;
                            if (new_y < 10) new_y = 10;
                            if (new_y > 180) new_y = 180;

                            dragged_window->x = new_x;
                            dragged_window->y = new_y;
                        }
                    } else {
                        // Release drag
                        dragged_window = NULL;
                    }
                    last_mouse_buttons = buttons;
                }
            } else { // Keyboard data
                uint8_t scancode = inb(0x60);
                if (scancode == 0x01) { // ESC key
                    return; // Exit GUI
                }
                if (window_terminal.active && window_terminal.visible) {
                    terminal_handle_key(scancode);
                }
            }
        }

        // Render frame
        redraw_desktop();

        // Delay to throttle loop (Vsync double buffer is self-throttled,
        // but a minor sleep loop prevents CPU pinning on virtual machines)
        for (volatile int d = 0; d < 0x2000; d++);
    }
}
