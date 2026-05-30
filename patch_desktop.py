import os

file_path = r"c:\Users\Harsh\OneDrive\Desktop\SQ-OS\kernel\desktop.c"

with open(file_path, 'r') as f:
    content = f.read()

# 1. Include snake.h
content = content.replace(
    '#include "../apps/notes.h"',
    '#include "../apps/notes.h"\n#include "../apps/snake.h"'
)

# 2. Add Taskbar Switcher
taskbar_str = """    draw_rect(0, 190, 320, 1, 15); // Bevel top highlight line

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
                if (win->title[j] == '\\0') { short_title[j]='\\0'; break; }
                short_title[j] = win->title[j];
            }
            short_title[5] = '\\0';
            draw_text(tb_x + 2, 192, short_title, txt_color);
            tb_x += 45;
            if (tb_x > 220) break; // Don't overlap clock
        }
    }"""
content = content.replace('    draw_rect(0, 190, 320, 1, 15); // Bevel top highlight line', taskbar_str)

# 3. Add Snake Icon
snake_icon_str = """    // Icon: TERMINAL (light red, X=15 Y=105)
    draw_icon_box(15, 105, 12);
    draw_text(6, 118, "TERMINAL", 15);

    // Icon: SNAKE (green, X=15 Y=145)
    draw_icon_box(15, 145, 10);
    draw_text(12, 158, "SNAKE", 15);"""
content = content.replace(
    '    // Icon: TERMINAL (light red, X=15 Y=105)\n    draw_icon_box(15, 105, 12);\n    draw_text(6, 118, "TERMINAL", 15);',
    snake_icon_str
)

# 4. Handle Taskbar Clicks
taskbar_click_str = """    // Check Taskbar clicks (Y: 191..199)
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

    // Check desktop icons if no window was clicked"""
content = content.replace('    // Check desktop icons if no window was clicked', taskbar_click_str)

# 5. Handle Snake Icon Click
snake_click_str = """        // TERMINAL icon: Y [100..130]
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
        }"""
content = content.replace(
    '        // TERMINAL icon: Y [100..130]\n        if (cy >= 100 && cy <= 130) {\n            window_terminal.visible = 1;\n            focus_window(&window_terminal);\n            return;\n        }',
    snake_click_str
)

# 6. Snake keyboard handling
snake_kb_str = """                } else if (window_calc.active && window_calc.visible) {
                    extern void calc_handle_key(uint8_t scancode);
                    calc_handle_key(scancode);
                } else if (window_snake.active && window_snake.visible) {
                    snake_handle_key(scancode);
                }"""
content = content.replace(
    '                } else if (window_calc.active && window_calc.visible) {\n                    extern void calc_handle_key(uint8_t scancode);\n                    calc_handle_key(scancode);\n                }',
    snake_kb_str
)

# 7. Add Uptime to Taskmanager
uptime_str = """    draw_text(win->x + 178, win->y + 88, "HEAP FREE", 8);
    draw_text(win->x + 178, win->y + 97, free_str, 0);

    extern volatile uint32_t system_ticks;
    uint32_t uptime_sec = system_ticks / 18;
    char up_str[16];
    desktop_u32_to_dec(uptime_sec, up_str);
    int ul = 0; while (up_str[ul]) ul++; up_str[ul++] = 's'; up_str[ul] = '\\0';
    
    draw_text(win->x + 178, win->y + 111, "UPTIME", 8);
    draw_text(win->x + 178, win->y + 120, up_str, 0);"""
content = content.replace(
    '    draw_text(win->x + 178, win->y + 88, "HEAP FREE", 8);\n    draw_text(win->x + 178, win->y + 97, free_str, 0);',
    uptime_str
)

with open(file_path, 'w') as f:
    f.write(content)

print("desktop.c patched successfully")
