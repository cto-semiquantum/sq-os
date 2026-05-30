#include "desktop.h"
#include "terminal_app.h"
#include "rtc.h"
#include "wallpaper.h"
#include "paging.h"
#include "syscall.h"
#include "process.h"
#include "memory.h"
#include "fat12.h"
#include "../apps/notes.h"
#include "../apps/snake.h"

static void desktop_u32_to_dec(uint32_t val, char *out);



static void draw_icon_box(int x, int y, uint8_t color) {
    draw_rect(x, y, 10, 10, color);
    draw_rect(x, y, 10, 1, 15);      // White highlight (top)
    draw_rect(x, y, 1, 10, 15);      // White highlight (left)
    draw_rect(x, y + 9, 10, 1, 8);   // Dark gray shadow (bottom)
    draw_rect(x + 9, y, 1, 10, 8);   // Dark gray shadow (right)
}

// Content Drawers
static void draw_sq_logo(int x, int y, uint8_t color, uint8_t shadow_color) {
    // 3D Pixel art representation for S and Q
    static const uint8_t logo_s[8] = {0x7E, 0x81, 0x80, 0x7C, 0x01, 0x81, 0x7E, 0x00};
    static const uint8_t logo_q[8] = {0x7E, 0x81, 0x81, 0x81, 0x8D, 0x83, 0x7E, 0x03};

    // S logo
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (logo_s[r] & (0x80 >> c)) {
                draw_rect(x + c + 1, y + r + 1, 1, 1, shadow_color);
                draw_rect(x + c, y + r, 1, 1, color);
            }
        }
    }
    // Q logo
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (logo_q[r] & (0x80 >> c)) {
                draw_rect(x + 10 + c + 1, y + r + 1, 1, 1, shadow_color);
                draw_rect(x + 10 + c, y + r, 1, 1, color);
            }
        }
    }
}

void draw_welcome_content(Window *win) {
    // White body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 15);
    // Dark gray separator line
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // Draw SQ Brand Logo (navy blue with light gray shadow)
    draw_sq_logo(win->x + 12, win->y + 24, 1, 7);

    // Text lines aligned to the right of the logo (shortened to prevent overflow)
    draw_text(win->x + 40, win->y + 20, "Welcome to SQ-OS!", 0);
    draw_text(win->x + 40, win->y + 34, "Arch: 32-bit PM", 8);
    draw_text(win->x + 40, win->y + 48, "Mem: Paging Active", 1); // Blue
    draw_text(win->x + 40, win->y + 62, "Syscalls: Active", 10);   // Green
}



void draw_files_content(Window *win) {
    // White body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 24, 15);
    // Separator line
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // Load actual FAT12 directory entries
    DirEntry entries[FAT12_MAX_FILES];
    int count = 0;
    if (fat12_init() == 0) {
        count = fat12_list_root(entries, FAT12_MAX_FILES);
    }

    int y_offset = 22;
    int files_drawn = 0;

    for (int j = 0; j < count; j++) {
        // Skip empty or deleted entries
        if (entries[j].name[0] == '\0' || entries[j].name[0] == 0xE5) continue;
        // Skip Volume ID attribute
        if (entries[j].attr & FAT_ATTR_VOLUME_ID) continue;

        if (y_offset < win->h - 14) {
            // Format 8.3 filename
            char name_str[16];
            int p = 0;
            for (int k = 0; k < 8 && entries[j].name[k] != ' '; k++) {
                name_str[p++] = entries[j].name[k];
            }
            if (entries[j].ext[0] != ' ') {
                name_str[p++] = '.';
                for (int k = 0; k < 3 && entries[j].ext[k] != ' '; k++) {
                    name_str[p++] = entries[j].ext[k];
                }
            }
            name_str[p] = '\0';

            // Draw file icon (green box with white bevel border)
            draw_rect(win->x + 10, win->y + y_offset, 6, 6, 10);
            draw_rect(win->x + 10, win->y + y_offset, 6, 1, 15);
            draw_rect(win->x + 10, win->y + y_offset, 1, 6, 15);

            // Draw filename (black)
            draw_text(win->x + 20, win->y + y_offset - 1, name_str, 0);

            // Draw size (gray text, e.g. "60B")
            char sz_str[15];
            desktop_u32_to_dec(entries[j].file_size, sz_str);
            int l = 0; while (sz_str[l]) l++; sz_str[l++] = 'B'; sz_str[l] = '\0';
            draw_text(win->x + 120, win->y + y_offset - 1, sz_str, 8);

            y_offset += 12;
            files_drawn++;
        }
    }

    if (files_drawn == 0) {
        draw_text(win->x + 15, win->y + 22, "No files found.", 8);
    }

    // Status bar (gray background)
    draw_rect(win->x + 4, win->y + win->h - 9, win->w - 8, 8, 7);

    char count_str[24];
    desktop_u32_to_dec(files_drawn, count_str);
    int cl = 0; while (count_str[cl]) cl++;
    const char *suffix = " files";
    int s = 0;
    while (suffix[s] && cl < 23) count_str[cl++] = suffix[s++];
    count_str[cl] = '\0';

    draw_text(win->x + 8, win->y + win->h - 9, count_str, 8);
}

uint8_t settings_mouse_speed = 1;
uint8_t temp_wallpaper_style = 0;
uint8_t temp_mouse_speed = 1;
uint8_t show_applied_text = 0;
int applied_text_tick = 0;

void draw_settings_content(Window *win) {
    // 1. Light gray body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 7);
    // Separator line under header
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // 2. Wallpaper style section
    draw_text(win->x + 10, win->y + 18, "WALLPAPER STYLE", 8); // Dark gray header

    // Option 1: Gradient
    if (temp_wallpaper_style == 0) {
        draw_text(win->x + 12, win->y + 28, "(x) Gradient", 1); // Blue selected
    } else {
        draw_text(win->x + 12, win->y + 28, "( ) Gradient", 0); // Black unselected
    }

    // Option 2: Solid Teal
    if (temp_wallpaper_style == 1) {
        draw_text(win->x + 12, win->y + 38, "(x) Solid Teal", 1);
    } else {
        draw_text(win->x + 12, win->y + 38, "( ) Solid Teal", 0);
    }

    // Option 3: Stars
    if (temp_wallpaper_style == 2) {
        draw_text(win->x + 12, win->y + 48, "(x) Starry Sky", 1);
    } else {
        draw_text(win->x + 12, win->y + 48, "( ) Starry Sky", 0);
    }

    // 3. Mouse speed section
    draw_text(win->x + 10, win->y + 62, "MOUSE SPEED", 8);

    // Option 1: Slow
    if (temp_mouse_speed == 0) {
        draw_text(win->x + 12, win->y + 72, "(x) Slow Speed", 1);
    } else {
        draw_text(win->x + 12, win->y + 72, "( ) Slow Speed", 0);
    }

    // Option 2: Normal
    if (temp_mouse_speed == 1) {
        draw_text(win->x + 12, win->y + 82, "(x) Normal Speed", 1);
    } else {
        draw_text(win->x + 12, win->y + 82, "( ) Normal Speed", 0);
    }

    // Option 3: Fast
    if (temp_mouse_speed == 2) {
        draw_text(win->x + 12, win->y + 92, "(x) Fast Speed", 1);
    } else {
        draw_text(win->x + 12, win->y + 92, "( ) Fast Speed", 0);
    }

    // 4. Apply button
    int btn_x = win->x + 12;
    int btn_y = win->y + 106;
    draw_rect(btn_x, btn_y, 45, 12, 7);
    draw_rect(btn_x, btn_y, 45, 1, 15);
    draw_rect(btn_x, btn_y, 1, 12, 15);
    draw_rect(btn_x, btn_y + 11, 45, 1, 8);
    draw_rect(btn_x + 44, btn_y, 1, 12, 8);
    draw_text(btn_x + 6, btn_y + 2, "APPLY", 0);

    // 5. Applied feedback text
    if (show_applied_text) {
        draw_text(win->x + 72, win->y + 108, "Applied!", 2); // Dark green feedback
        applied_text_tick++;
        if (applied_text_tick > 100) {
            show_applied_text = 0;
            applied_text_tick = 0;
        }
    }
}

static void draw_3d_block(int x, int y, int w, int h, uint8_t color, uint8_t highlight, uint8_t shadow) {
    draw_rect(x, y, w, h, color);
    draw_rect(x, y, w, 1, highlight);
    draw_rect(x, y, 1, h, highlight);
    draw_rect(x, y + h - 1, w, 1, shadow);
    draw_rect(x + w - 1, y, 1, h, shadow);
}

void draw_large_3d_logo(void) {
    // Draw SQ Logo in the center of the desktop background
    // S Logo blocks
    draw_3d_block(120, 70, 28, 8, 11, 15, 3);   // Top
    draw_3d_block(120, 78, 8, 8, 11, 15, 3);    // Upper left
    draw_3d_block(120, 86, 28, 8, 11, 15, 3);   // Middle
    draw_3d_block(140, 94, 8, 8, 11, 15, 3);    // Lower right
    draw_3d_block(120, 102, 28, 8, 11, 15, 3);  // Bottom

    // Q Logo blocks
    draw_3d_block(156, 70, 8, 40, 11, 15, 3);   // Left
    draw_3d_block(176, 70, 8, 32, 11, 15, 3);   // Right
    draw_3d_block(164, 70, 12, 8, 11, 15, 3);   // Top
    draw_3d_block(164, 102, 12, 8, 11, 15, 3);  // Bottom
    draw_3d_block(172, 94, 8, 8, 11, 15, 3);    // Tail connection
    draw_3d_block(180, 102, 8, 10, 11, 15, 3);  // Tail end
}

// Redraw entire desktop
void redraw_desktop(void) {
    // 1. Clear backbuffer to black
    clear_backbuffer();

    // 2. Wallpaper background (retro gradient — covers full 320x200)
    render_wallpaper();

    // Draw large 3D SQ logo on background
    draw_large_3d_logo();

    // 3. Top bar (cyan, Y=0..9)
    draw_rect(0, 0, 320, 10, 3);
    // Draw top status bar logo
    draw_sq_logo(4, 1, 15, 8);
    draw_text(110, 1, "SQ-OS DESKTOP v3.0", 15);


    // 4. Taskbar (retro light gray, Y=190..199)
    draw_rect(0, 190, 320, 10, 7);
    draw_rect(0, 190, 320, 1, 15); // Bevel top highlight line

    // Draw Taskbar App Switcher
    int tb_x = 70;
    for (int i = 0; i < NUM_WINDOWS; i++) {
        Window *win = window_order[i];
        if (win->visible && win != &window_welcome) {
            uint8_t btn_color = win->active ? 15 : 7;
            uint8_t txt_color = win->active ? 0 : 8;
            draw_rect(tb_x, 191, 40, 8, btn_color);
            draw_rect(tb_x, 191, 40, 1, 15);
            draw_rect(tb_x, 191, 1, 8, 15);
            draw_rect(tb_x, 198, 40, 1, 8);
            draw_rect(tb_x + 39, 191, 1, 8, 8);
            
            char short_title[6];
            for(int j=0; j<5; j++) {
                if (win->title[j] == '\0') { short_title[j]='\0'; break; }
                short_title[j] = win->title[j];
            }
            short_title[5] = '\0';
            draw_text(tb_x + 2, 192, short_title, txt_color);
            tb_x += 45;
            if (tb_x > 220) break; // Don't overlap clock
        }
    }

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
    // Column 1 (X=15)
    // Icon: SETTINGS (yellow, X=15 Y=25)
    draw_icon_box(15, 25, 14);
    draw_text(6, 38, "SETTINGS", 15);

    // Icon: FILES (green, X=15 Y=65)
    draw_icon_box(15, 65, 10);
    draw_text(12, 78, "FILES", 15);

    // Icon: TERMINAL (light red, X=15 Y=105)
    draw_icon_box(15, 105, 12);
    draw_text(6, 118, "TERMINAL", 15);

    // Icon: SNAKE (green, X=15 Y=145)
    draw_icon_box(15, 145, 10);
    draw_text(12, 158, "SNAKE", 15);

    // Column 2 (X=85)
    // Icon: MONITOR (magenta, X=85 Y=25)
    draw_icon_box(85, 25, 13);
    draw_text(82, 38, "TASKS", 15);

    // Icon: NOTES (light blue, X=85 Y=65)
    draw_icon_box(85, 65, 11);
    draw_text(82, 78, "NOTES", 15);

    // Icon: CALC (orange/red, X=85 Y=105)
    draw_icon_box(85, 105, 12);
    draw_text(85, 118, "CALC", 15);

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

            if (win == &window_notes) {
                int rel_x = cx - win->x;
                int rel_y = cy - win->y;
                if (rel_y >= 98 && rel_y <= 110) {
                    if (rel_x >= 10 && rel_x <= 50) {
                        notes_open();
                    } else if (rel_x >= 60 && rel_x <= 100) {
                        notes_save();
                    } else if (rel_x >= 105 && rel_x <= 150) {
                        notes_clear();
                    }
                }
            }

            if (win == &window_files) {
                int rel_x = cx - win->x;
                int rel_y = cy - win->y;
                if (rel_x >= 10 && rel_x < win->w - 10) {
                    DirEntry entries[FAT12_MAX_FILES];
                    int count = 0;
                    if (fat12_init() == 0) {
                        count = fat12_list_root(entries, FAT12_MAX_FILES);
                    }
                    int clicked_idx = (rel_y - 22) / 12;
                    int click_rem = (rel_y - 22) % 12;
                    if (clicked_idx >= 0 && clicked_idx < count && click_rem < 10) {
                        DirEntry *clicked_entry = &entries[clicked_idx];
                        char filename[16];
                        int p = 0;
                        for (int k = 0; k < 8 && clicked_entry->name[k] != ' '; k++) {
                            filename[p++] = clicked_entry->name[k];
                        }
                        if (clicked_entry->ext[0] != ' ') {
                            filename[p++] = '.';
                            for (int k = 0; k < 3 && clicked_entry->ext[k] != ' '; k++) {
                                filename[p++] = clicked_entry->ext[k];
                            }
                        }
                        filename[p] = '\0';

                        // If HELLO.APP, run it in TERMINAL
                        if (clicked_entry->ext[0] == 'A' && clicked_entry->ext[1] == 'P' && clicked_entry->ext[2] == 'P') {
                            window_terminal.visible = 1;
                            focus_window(&window_terminal);
                            char cmd_buf[32] = "run ";
                            int clen = 4;
                            for (int k = 0; filename[k]; k++) cmd_buf[clen++] = filename[k];
                            cmd_buf[clen] = '\0';
                            terminal_execute_command(cmd_buf);
                        }
                        // If .TXT, open in NOTES
                        else if (clicked_entry->ext[0] == 'T' && clicked_entry->ext[1] == 'X' && clicked_entry->ext[2] == 'T') {
                            window_notes.visible = 1;
                            focus_window(&window_notes);
                            int read_bytes = fat12_read_file(clicked_entry, (uint8_t *)notes_text, NOTES_MAX_SIZE - 1);
                            if (read_bytes >= 0) {
                                notes_text[read_bytes] = '\0';
                                notes_text_len = (uint32_t)read_bytes;
                            } else {
                                notes_text[0] = '\0';
                                notes_text_len = 0;
                            }
                        }
                    }
                }
            }

            if (win == &window_calc) {
                int rel_x = cx - win->x;
                int rel_y = cy - win->y;
                if (rel_y >= 36 && rel_y <= 110) {
                    int row = -1;
                    if (rel_y >= 36 && rel_y <= 50) row = 0;
                    else if (rel_y >= 56 && rel_y <= 70) row = 1;
                    else if (rel_y >= 76 && rel_y <= 90) row = 2;
                    else if (rel_y >= 96 && rel_y <= 110) row = 3;

                    int col = -1;
                    if (rel_x >= 6 && rel_x <= 30) col = 0;
                    else if (rel_x >= 40 && rel_x <= 64) col = 1;
                    else if (rel_x >= 74 && rel_x <= 98) col = 2;
                    else if (rel_x >= 108 && rel_x <= 132) col = 3;

                    if (row != -1 && col != -1) {
                        char btn_chars[4][4] = {
                            {'7', '8', '9', '/'},
                            {'4', '5', '6', '*'},
                            {'1', '2', '3', '-'},
                            {'C', '0', '=', '+'}
                        };
                        calc_press(btn_chars[row][col]);
                    }
                }
            }

            if (win == &window_settings) {
                int rel_x = cx - win->x;
                int rel_y = cy - win->y;

                // Wallpaper options (X: 12..120)
                if (rel_x >= 12 && rel_x <= 120) {
                    extern uint8_t temp_wallpaper_style;
                    if (rel_y >= 28 && rel_y < 38) {
                        temp_wallpaper_style = 0;
                    } else if (rel_y >= 38 && rel_y < 48) {
                        temp_wallpaper_style = 1;
                    } else if (rel_y >= 48 && rel_y < 58) {
                        temp_wallpaper_style = 2;
                    }
                }

                // Mouse speed options (X: 12..120)
                if (rel_x >= 12 && rel_x <= 120) {
                    extern uint8_t temp_mouse_speed;
                    if (rel_y >= 72 && rel_y < 82) {
                        temp_mouse_speed = 0;
                    } else if (rel_y >= 82 && rel_y < 92) {
                        temp_mouse_speed = 1;
                    } else if (rel_y >= 92 && rel_y < 102) {
                        temp_mouse_speed = 2;
                    }
                }

                // Apply button (Y: 106..118, X: 12..57)
                if (rel_y >= 106 && rel_y <= 118) {
                    if (rel_x >= 12 && rel_x <= 57) {
                        extern uint8_t settings_wallpaper_style;
                        extern uint8_t settings_mouse_speed;
                        extern uint8_t temp_wallpaper_style;
                        extern uint8_t temp_mouse_speed;
                        extern uint8_t show_applied_text;
                        extern int applied_text_tick;

                        settings_wallpaper_style = temp_wallpaper_style;
                        settings_mouse_speed = temp_mouse_speed;
                        show_applied_text = 1;
                        applied_text_tick = 0;
                    }
                }
            }

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

    // Check Taskbar clicks (Y: 191..199)
    if (cy >= 191 && cy <= 199) {
        int tb_x = 70;
        for (int i = 0; i < NUM_WINDOWS; i++) {
            Window *win = window_order[i];
            if (win->visible && win != &window_welcome) {
                if (cx >= tb_x && cx < tb_x + 40) {
                    focus_window(win);
                    return;
                }
                tb_x += 45;
                if (tb_x > 220) break;
            }
        }
    }

    // Check desktop icons if no window was clicked
    // Column 1 Icons (X [5..55])
    if (cx >= 5 && cx <= 55) {
        // SETTINGS icon: Y [20..50]
        if (cy >= 20 && cy <= 50) {
            window_settings.visible = 1;
            focus_window(&window_settings);
            return;
        }
        // FILES icon: Y [60..90]
        if (cy >= 60 && cy <= 90) {
            window_files.visible = 1;
            focus_window(&window_files);
            return;
        }
        // TERMINAL icon: Y [100..130]
        if (cy >= 100 && cy <= 130) {
            window_terminal.visible = 1;
            focus_window(&window_terminal);
            return;
        }
        // SNAKE icon: Y [140..170]
        if (cy >= 140 && cy <= 170) {
            window_snake.visible = 1;
            focus_window(&window_snake);
            snake_init();
            return;
        }
    }
    
    // Column 2 Icons (X [75..115])
    if (cx >= 75 && cx <= 115) {
        // MONITOR icon: Y [20..50]
        if (cy >= 20 && cy <= 50) {
            window_taskmanager.visible = 1;
            focus_window(&window_taskmanager);
            return;
        }
        // NOTES icon: Y [60..90]
        if (cy >= 60 && cy <= 90) {
            window_notes.visible = 1;
            focus_window(&window_notes);
            return;
        }
        // CALC icon: Y [100..130]
        if (cy >= 100 && cy <= 130) {
            window_calc.visible = 1;
            focus_window(&window_calc);
            return;
        }
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

                    extern uint8_t settings_mouse_speed;
                    if (settings_mouse_speed == 0) {
                        dx /= 2;
                        dy /= 2;
                    } else if (settings_mouse_speed == 2) {
                        dx *= 2;
                        dy *= 2;
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
                } else if (window_notes.active && window_notes.visible) {
                    notes_handle_key(scancode);
                } else if (window_calc.active && window_calc.visible) {
                    extern void calc_handle_key(uint8_t scancode);
                    calc_handle_key(scancode);
                } else if (window_snake.active && window_snake.visible) {
                    snake_handle_key(scancode);
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

// ------------------------------------------------------------------
// draw_taskmanager_content — Task Manager and System Monitor Drawer
// ------------------------------------------------------------------
static void desktop_u32_to_dec(uint32_t val, char *out) {
    if (val == 0) { out[0]='0'; out[1]='\0'; return; }
    char tmp[12]; int n = 0;
    while (val > 0) { tmp[n++] = '0' + (val % 10); val /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
}

void draw_taskmanager_content(Window *win) {
    // 1. Draw body (white background)
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 15);
    
    // 2. Separator line under header
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // 3. Draw column titles (PID, NAME, STATUS, SIZE) - Aligned to 8px character boundaries
    draw_text(win->x, win->y + 17, " PIDNAME   STAT  SIZE", 8);

    // 4. Print running processes
    int y_offset = 27;
    int proc_found = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_UNUSED && y_offset < win->h - 10) {
            proc_found = 1;
            
            // PID - starts at 8px (char index 1)
            char id_str[6];
            desktop_u32_to_dec(process_table[i].id, id_str);
            draw_text(win->x + 8, win->y + y_offset, id_str, 0);

            // Name - starts at 32px (char index 4)
            char name_tmp[7];
            int name_len = 0;
            while (process_table[i].name[name_len] && name_len < 6) {
                name_tmp[name_len] = process_table[i].name[name_len];
                name_len++;
            }
            name_tmp[name_len] = '\0';
            draw_text(win->x + 32, win->y + y_offset, name_tmp, 0);

            // State (CREAT, RUN, TERM) - starts at 88px (char index 11)
            const char *st_str = "UNK";
            if (process_table[i].state == PROC_STATE_CREATED) st_str = "CREAT";
            else if (process_table[i].state == PROC_STATE_RUNNING) st_str = "RUN  ";
            else if (process_table[i].state == PROC_STATE_TERMINATED) st_str = "TERM ";
            draw_text(win->x + 88, win->y + y_offset, st_str, 1); // Blue text

            // Size - starts at 136px (char index 17)
            char sz_str[15];
            desktop_u32_to_dec(process_table[i].size, sz_str);
            int l = 0; while (sz_str[l]) l++; sz_str[l] = 'B'; sz_str[l+1] = '\0';
            draw_text(win->x + 136, win->y + y_offset, sz_str, 0);

            y_offset += 10;
        }
    }
    
    if (!proc_found) {
        draw_text(win->x + 8, win->y + 35, "No active processes", 8);
    }

    // 5. Draw Vertical Separator line (shifted from 152 to 172 to prevent overlaps)
    draw_rect(win->x + 172, win->y + 14, 1, win->h - 18, 8);

    // 6. Draw System Stats on the Right (shifted from 158 to 178 to align inside right pane)
    draw_text(win->x + 178, win->y + 17, "SYSTEM", 1); // Blue heading

    // Paging status
    if (is_paging_enabled()) {
        draw_text(win->x + 178, win->y + 28, "PG: ON", 10); // Green ON
    } else {
        draw_text(win->x + 178, win->y + 28, "PG: OFF", 12); // Red OFF
    }

    // Heap stats
    char heap_str[24];
    desktop_u32_to_dec((uint32_t)heap_used(), heap_str);
    int hl = 0; while (heap_str[hl]) hl++; heap_str[hl++] = 'B'; heap_str[hl] = '\0';
    
    draw_text(win->x + 178, win->y + 42, "HEAP USED", 8);
    draw_text(win->x + 178, win->y + 51, heap_str, 0);

    // Syscalls stats
    uint32_t total_syscalls = 0;
    for (int k = 1; k < 6; k++) {
        total_syscalls += get_syscall_count(k);
    }
    char sys_str[16];
    desktop_u32_to_dec(total_syscalls, sys_str);
    
    draw_text(win->x + 178, win->y + 65, "SYSCALLS", 8);
    draw_text(win->x + 178, win->y + 74, sys_str, 0);
    
    // Heap Free stats
    HeapStats hs;
    heap_stats(&hs);
    char free_str[24];
    desktop_u32_to_dec(hs.free_bytes, free_str);
    int fl = 0; while (free_str[fl]) fl++; free_str[fl++] = 'B'; free_str[fl] = '\0';
    
    draw_text(win->x + 178, win->y + 88, "HEAP FREE", 8);
    draw_text(win->x + 178, win->y + 97, free_str, 0);

    extern volatile uint32_t system_ticks;
    uint32_t uptime_sec = system_ticks / 18;
    char up_str[16];
    desktop_u32_to_dec(uptime_sec, up_str);
    int ul = 0; while (up_str[ul]) ul++; up_str[ul++] = 's'; up_str[ul] = '\0';
    
    draw_text(win->x + 178, win->y + 111, "UPTIME", 8);
    draw_text(win->x + 178, win->y + 120, up_str, 0);
}

// ------------------------------------------------------------------
// SQ Calculator Application Implementation
// ------------------------------------------------------------------
char calc_display[32] = "";
int calc_val1 = 0;
int calc_val2 = 0;
char calc_op = 0;
int calc_has_result = 0;

void calc_press(char c) {
    if (c >= '0' && c <= '9') {
        if (calc_has_result) {
            calc_display[0] = '\0';
            calc_has_result = 0;
        }
        int len = 0;
        while (calc_display[len]) len++;
        if (len < 15) {
            calc_display[len] = c;
            calc_display[len+1] = '\0';
        }
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_val1 = 0;
        int i = 0;
        int sign = 1;
        if (calc_display[0] == '-') {
            sign = -1;
            i++;
        }
        while (calc_display[i]) {
            if (calc_display[i] >= '0' && calc_display[i] <= '9') {
                calc_val1 = calc_val1 * 10 + (calc_display[i] - '0');
            }
            i++;
        }
        calc_val1 *= sign;
        calc_op = c;
        calc_display[0] = '\0';
        calc_has_result = 0;
    } else if (c == '=') {
        if (calc_op == 0) return;
        calc_val2 = 0;
        int i = 0;
        int sign = 1;
        if (calc_display[0] == '-') {
            sign = -1;
            i++;
        }
        while (calc_display[i]) {
            if (calc_display[i] >= '0' && calc_display[i] <= '9') {
                calc_val2 = calc_val2 * 10 + (calc_display[i] - '0');
            }
            i++;
        }
        calc_val2 *= sign;

        int res = 0;
        if (calc_op == '+') res = calc_val1 + calc_val2;
        else if (calc_op == '-') res = calc_val1 - calc_val2;
        else if (calc_op == '*') res = calc_val1 * calc_val2;
        else if (calc_op == '/') {
            if (calc_val2 == 0) {
                calc_display[0] = 'E';
                calc_display[1] = 'r';
                calc_display[2] = 'r';
                calc_display[3] = '\0';
                calc_op = 0;
                calc_has_result = 1;
                return;
            }
            res = calc_val1 / calc_val2;
        }

        int temp = res;
        int is_neg = 0;
        if (temp < 0) {
            is_neg = 1;
            temp = -temp;
        }
        char tmp_str[16];
        int n = 0;
        if (temp == 0) {
            tmp_str[n++] = '0';
        } else {
            while (temp > 0) {
                tmp_str[n++] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        int p = 0;
        if (is_neg) {
            calc_display[p++] = '-';
        }
        for (int j = 0; j < n; j++) {
            calc_display[p++] = tmp_str[n - 1 - j];
        }
        calc_display[p] = '\0';

        calc_op = 0;
        calc_has_result = 1;
    } else if (c == 'C') {
        calc_display[0] = '\0';
        calc_val1 = 0;
        calc_val2 = 0;
        calc_op = 0;
        calc_has_result = 0;
    }
}

void draw_calc_content(Window *win) {
    // 1. White body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 15);
    // Separator line under header
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // 2. Display box
    int box_x = win->x + 8;
    int box_y = win->y + 18;
    int box_w = win->w - 16;
    int box_h = 14;
    draw_rect(box_x, box_y, box_w, box_h, 7);
    draw_rect(box_x, box_y, box_w, 1, 8);
    draw_rect(box_x, box_y, 1, box_h, 8);
    draw_rect(box_x, box_y + box_h - 1, box_w, 1, 15);
    draw_rect(box_x + box_w - 1, box_y, 1, box_h, 15);

    int len = 0;
    while (calc_display[len]) len++;
    int text_x = box_x + box_w - 8 - len * 6;
    if (text_x < box_x + 4) text_x = box_x + 4;
    draw_text(text_x, box_y + 3, calc_display, 0);

    // 3. Draw buttons
    char btn_chars[4][4] = {
        {'7', '8', '9', '/'},
        {'4', '5', '6', '*'},
        {'1', '2', '3', '-'},
        {'C', '0', '=', '+'}
    };

    for (int r = 0; r < 4; r++) {
        int by = win->y + 36 + r * 20;
        for (int c = 0; c < 4; c++) {
            int bx = win->x + 6 + c * 34;
            
            draw_rect(bx, by, 24, 14, 7);
            draw_rect(bx, by, 24, 1, 15);
            draw_rect(bx, by, 1, 14, 15);
            draw_rect(bx, by + 13, 24, 1, 8);
            draw_rect(bx + 23, by, 1, 14, 8);

            char str[2] = {btn_chars[r][c], '\0'};
            uint8_t text_color = 0;
            if (btn_chars[r][c] == 'C') text_color = 12;
            else if (btn_chars[r][c] == '=') text_color = 2;
            else if (btn_chars[r][c] == '+' || btn_chars[r][c] == '-' || btn_chars[r][c] == '*' || btn_chars[r][c] == '/') text_color = 1;

            draw_text(bx + 9, by + 3, str, text_color);
        }
    }
}

void calc_handle_key(uint8_t scancode) {
    if (scancode & 0x80) return;
    
    char c = 0;
    if (scancode == 0x0B) c = '0';
    else if (scancode >= 0x02 && scancode <= 0x0A) c = '1' + (scancode - 0x02);
    else if (scancode == 0x0C) c = '-';
    else if (scancode == 0x0D) c = '=';
    else if (scancode == 0x1C) c = '=';
    else if (scancode == 0x2E) c = 'C';
    else if (scancode == 0x37) c = '*';
    else if (scancode == 0x4E) c = '+';
    else if (scancode == 0x4A) c = '-';
    else if (scancode == 0x35) c = '/';
    
    if (c) calc_press(c);
}


