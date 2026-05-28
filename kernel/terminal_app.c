#include "terminal_app.h"

char term_input[64];
uint32_t term_input_len = 0;
char term_history[5][32];

// Local Keyboard Scancode-to-ASCII map
static const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static void str_copy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void init_terminal_app(void) {
    term_input_len = 0;
    term_input[0] = '\0';

    // Clear history to blank strings
    for (int i = 0; i < 5; i++) {
        term_history[i][0] = '\0';
    }

    // Set greeting lines
    str_copy(term_history[2], "SQ-OS Terminal v2.0", 32);
    str_copy(term_history[3], "Arch: 32-bit Protected Mode", 32);
    str_copy(term_history[4], "Type HELP for commands", 32);
}

void append_history(const char *line) {
    // Scroll previous history lines up (0 gets overwritten, 1->0, 2->1, 3->2, 4->3)
    for (int i = 0; i < 4; i++) {
        str_copy(term_history[i], term_history[i + 1], 32);
    }
    // Copy new line to the last row (line 4)
    str_copy(term_history[4], line, 32);
}

void terminal_execute_command(const char *cmd) {
    // 1. Construct prompt command line and append it to history
    char cmd_line[32];
    str_copy(cmd_line, "SQ> ", 32);
    // Append command to "SQ> "
    int len = 4;
    while (cmd[len - 4] != '\0' && len < 31) {
        cmd_line[len] = cmd[len - 4];
        len++;
    }
    cmd_line[len] = '\0';
    append_history(cmd_line);

    // 2. Parse and execute command
    if (strcmp(cmd, "help") == 0) {
        append_history("Commands: HELP, ABOUT,");
        append_history("VERSION, CLEAR, REBOOT");
    } else if (strcmp(cmd, "about") == 0) {
        append_history("SQ-OS by Harsh");
        append_history("Freestanding hybrid C kernel");
    } else if (strcmp(cmd, "version") == 0) {
        append_history("SQ-OS v2.0 (Hybrid C+ASM)");
    } else if (strcmp(cmd, "clear") == 0) {
        for (int i = 0; i < 5; i++) {
            term_history[i][0] = '\0';
        }
    } else if (strcmp(cmd, "reboot") == 0) {
        append_history("Rebooting...");
        // Output reboot to 8042 keyboard controller
        outb(0x64, 0xFE);
        while (1) {
            __asm__ volatile("hlt");
        }
    } else if (strcmp(cmd, "") == 0) {
        // Empty command, do nothing
    } else {
        append_history("Unknown command");
    }
}

void terminal_handle_key(uint8_t scancode) {
    if (scancode & 0x80) return; // Release scancode
    if (scancode >= 128) return;

    char c = scancode_map[scancode];
    if (c == 0) return;

    if (c == '\b') {
        if (term_input_len > 0) {
            term_input_len--;
            term_input[term_input_len] = '\0';
        }
    } else if (c == '\n') {
        term_input[term_input_len] = '\0';
        terminal_execute_command(term_input);
        term_input_len = 0;
        term_input[0] = '\0';
    } else {
        // Enforce maximum size of 18 chars to fit within 200px window
        if (term_input_len < 18) {
            term_input[term_input_len] = c;
            term_input_len++;
            term_input[term_input_len] = '\0';
        }
    }
}

void draw_terminal_content(Window *win) {
    // 1. Draw black background area
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 0);

    // 2. Draw history lines (light green, color 10)
    for (int i = 0; i < 5; i++) {
        if (term_history[i][0] != '\0') {
            draw_text(win->x + 8, win->y + 20 + (i * 12), term_history[i], 10);
        }
    }

    // 3. Draw active input prompt line with blinking block cursor
    char prompt_line[32];
    str_copy(prompt_line, "SQ> ", 32);
    int len = 4;
    while (term_input[len - 4] != '\0' && len < 30) {
        prompt_line[len] = term_input[len - 4];
        len++;
    }
    prompt_line[len] = '_';
    prompt_line[len + 1] = '\0';

    draw_text(win->x + 8, win->y + 80, prompt_line, 10);
}
