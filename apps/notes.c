#include "notes.h"
#include "graphics.h"
#include "fat12.h"

char notes_text[NOTES_MAX_SIZE] = "";
uint32_t notes_text_len = 0;

static char notes_status[32] = "";

void notes_open(void) {
    int read_bytes = fs_read_file("NOTES.TXT", (uint8_t *)notes_text, NOTES_MAX_SIZE - 1);
    if (read_bytes >= 0) {
        notes_text[read_bytes] = '\0';
        notes_text_len = (uint32_t)read_bytes;
        // Copy string safely without standard library dependencies
        const char *src = "Loaded successfully";
        int i = 0;
        while (src[i] && i < 31) { notes_status[i] = src[i]; i++; }
        notes_status[i] = '\0';
    } else {
        const char *src = "No note found";
        int i = 0;
        while (src[i] && i < 31) { notes_status[i] = src[i]; i++; }
        notes_status[i] = '\0';
    }
}

void notes_save(void) {
    int written = fs_write_file("NOTES.TXT", (const uint8_t *)notes_text, notes_text_len);
    if (written >= 0) {
        const char *src = "Saved successfully";
        int i = 0;
        while (src[i] && i < 31) { notes_status[i] = src[i]; i++; }
        notes_status[i] = '\0';
    } else {
        const char *src = "Save failed";
        int i = 0;
        while (src[i] && i < 31) { notes_status[i] = src[i]; i++; }
        notes_status[i] = '\0';
    }
}

void notes_clear(void) {
    notes_text[0] = '\0';
    notes_text_len = 0;
    notes_status[0] = '\0';
}

void notes_handle_key(uint8_t scancode) {
    if (scancode & 0x80) return; // ignore release
    if (scancode >= 128) return;

    // Clear status message when user types
    notes_status[0] = '\0';

    static const char notes_scancode_map[128] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
    };

    char c = notes_scancode_map[scancode];
    if (c == 0) return;

    if (c == '\b') {
        if (notes_text_len > 0) {
            notes_text_len--;
            notes_text[notes_text_len] = '\0';
        }
    } else if (c == '\n') {
        if (notes_text_len < (NOTES_MAX_SIZE - 2)) {
            notes_text[notes_text_len++] = '\n';
            notes_text[notes_text_len] = '\0';
        }
    } else {
        if (notes_text_len < (NOTES_MAX_SIZE - 2)) {
            notes_text[notes_text_len++] = c;
            notes_text[notes_text_len] = '\0';
        }
    }
}

void draw_notes_content(Window *win) {
    // 1. Draw body (white)
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 15);
    
    // 2. Separator line under header
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8);

    // 3. Draw text area border (dark gray)
    int box_x = win->x + 8;
    int box_y = win->y + 18;
    int box_w = win->w - 16;
    int box_h = 75;
    draw_rect(box_x, box_y, box_w, 1, 8);
    draw_rect(box_x, box_y, 1, box_h, 8);
    draw_rect(box_x, box_y + box_h - 1, box_w, 1, 8);
    draw_rect(box_x + box_w - 1, box_y, 1, box_h, 8);

    // 4. Render text with clipping bounds
    int min_x = box_x + 3;
    int max_x = box_x + box_w - 3;
    int min_y = box_y + 3;
    int max_y = box_y + box_h - 3;

    char line_buf[40];
    int line_len = 0;
    int cur_y = box_y + 4;
    
    for (int i = 0; i <= (int)notes_text_len; i++) {
        char c = notes_text[i];
        if (c == '\n' || c == '\0' || line_len >= 30) {
            line_buf[line_len] = '\0';
            draw_text_clipped(box_x + 4, cur_y, line_buf, 0, min_x, max_x, min_y, max_y);
            cur_y += 10;
            line_len = 0;
            if (c == '\0') break;
        } else {
            line_buf[line_len++] = c;
        }
    }

    // 5. Draw blinking cursor
    static int cursor_tick = 0;
    cursor_tick++;
    if ((cursor_tick & 31) < 20) {
        int last_line_len = 0;
        int last_line_y = box_y + 4;
        for (int i = 0; i < (int)notes_text_len; i++) {
            if (notes_text[i] == '\n') {
                last_line_len = 0;
                last_line_y += 10;
            } else {
                last_line_len++;
            }
        }
        int cursor_x = box_x + 4 + last_line_len * 6;
        if (cursor_x < max_x - 6 && last_line_y < max_y - 6) {
            draw_text_clipped(cursor_x, last_line_y, "_", 0, min_x, max_x, min_y, max_y);
        }
    }

    // 6. Draw OPEN, SAVE, and CLEAR buttons
    int btn_y = win->y + 98;
    
    // OPEN button
    draw_rect(win->x + 10, btn_y, 40, 12, 7);
    draw_rect(win->x + 10, btn_y, 40, 1, 15);
    draw_rect(win->x + 10, btn_y, 1, 12, 15);
    draw_rect(win->x + 10, btn_y + 11, 40, 1, 8);
    draw_rect(win->x + 49, btn_y, 1, 12, 8);
    draw_text(win->x + 16, btn_y + 2, "OPEN", 0);

    // SAVE button
    draw_rect(win->x + 60, btn_y, 40, 12, 7);
    draw_rect(win->x + 60, btn_y, 40, 1, 15);
    draw_rect(win->x + 60, btn_y, 1, 12, 15);
    draw_rect(win->x + 60, btn_y + 11, 40, 1, 8);
    draw_rect(win->x + 99, btn_y, 1, 12, 8);
    draw_text(win->x + 66, btn_y + 2, "SAVE", 0);

    // CLEAR button
    draw_rect(win->x + 105, btn_y, 45, 12, 7);
    draw_rect(win->x + 105, btn_y, 45, 1, 15);
    draw_rect(win->x + 105, btn_y, 1, 12, 15);
    draw_rect(win->x + 105, btn_y + 11, 45, 1, 8);
    draw_rect(win->x + 149, btn_y, 1, 12, 8);
    draw_text(win->x + 109, btn_y + 2, "CLEAR", 0);

    // 7. Draw status message
    if (notes_status[0] != '\0') {
        // Draw status at the bottom in red (color 12) or blue (color 1)
        uint8_t status_color = 12; // default red
        if (notes_status[0] == 'S' || notes_status[0] == 'L') {
            status_color = 1; // blue on success
        }
        draw_text(win->x + 10, win->y + 114, notes_status, status_color);
    }
}
