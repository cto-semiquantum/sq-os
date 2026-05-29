#include "terminal_app.h"
#include "../fs/fat12.h"
#include "memory.h"
#include "loader.h"
#include "paging.h"
#include "syscall.h"
#include "process.h"

/* ============================================================
 * Terminal State
 * ============================================================ */
char     term_input[TERM_INPUT_MAX + 2];
uint32_t term_input_len = 0;

/* Output history — circular, 8 lines of 40 chars */
char term_history[TERM_HISTORY_LINES][TERM_HISTORY_LEN];

/* Command recall history (Up/Down arrow) */
static char   cmd_hist[TERM_CMD_HIST_SIZE][TERM_INPUT_MAX + 1];
static int    cmd_hist_count = 0;   /* total entries stored          */
static int    cmd_hist_idx   = -1;  /* -1 = not browsing history     */

/* Blinking cursor tick (toggled each frame via draw_terminal_content) */
static int cursor_blink_tick = 0;

/* ============================================================
 * Keyboard Scancode → ASCII (identical map to kernel.c)
 * ============================================================ */
static const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

/* ============================================================
 * Internal string helpers (no libc)
 * ============================================================ */
static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void str_copy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0');
}

/* Returns 1 if str starts with prefix */
static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

/* Convert uint32_t to decimal string, returns length */
static int u32_to_dec(uint32_t val, char *out) {
    if (val == 0) { out[0]='0'; out[1]='\0'; return 1; }
    char tmp[12]; int n = 0;
    while (val > 0) { tmp[n++] = '0' + (val % 10); val /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = '\0';
    return n;
}

/* ============================================================
 * Output history helpers
 * ============================================================ */

/* append_history — scroll all lines up by one, place new line at bottom */
void append_history(const char *line) {
    for (int i = 0; i < TERM_HISTORY_LINES - 1; i++) {
        str_copy(term_history[i], term_history[i + 1], TERM_HISTORY_LEN);
    }
    str_copy(term_history[TERM_HISTORY_LINES - 1], line, TERM_HISTORY_LEN);
}

/* ============================================================
 * Command recall history
 * ============================================================ */
static void push_cmd_hist(const char *cmd) {
    if (cmd[0] == '\0') return; /* don't save empty lines */
    /* Shift entries up, drop the oldest if full */
    if (cmd_hist_count < TERM_CMD_HIST_SIZE) cmd_hist_count++;
    for (int i = cmd_hist_count - 1; i > 0; i--) {
        str_copy(cmd_hist[i], cmd_hist[i-1], TERM_INPUT_MAX + 1);
    }
    str_copy(cmd_hist[0], cmd, TERM_INPUT_MAX + 1);
    cmd_hist_idx = -1;
}

/* ============================================================
 * init_terminal_app
 * ============================================================ */
void init_terminal_app(void) {
    term_input_len = 0;
    term_input[0]  = '\0';
    cmd_hist_count = 0;
    cmd_hist_idx   = -1;

    /* Clear all history lines */
    for (int i = 0; i < TERM_HISTORY_LINES; i++) {
        term_history[i][0] = '\0';
    }

    /* Greeting messages */
    append_history("SQ-OS Terminal v3.0");
    append_history("32-bit Protected Mode");
    append_history("Type HELP for commands");
    append_history("");
}

/* ============================================================
 * terminal_execute_command
 * ============================================================ */
void terminal_execute_command(const char *cmd) {
    /* Show the entered command with prompt prefix */
    char cmd_line[TERM_HISTORY_LEN];
    str_copy(cmd_line, "SQ> ", TERM_HISTORY_LEN);
    int offset = str_len("SQ> ");
    int i = 0;
    while (cmd[i] != '\0' && offset + i < TERM_HISTORY_LEN - 1) {
        cmd_line[offset + i] = cmd[i];
        i++;
    }
    cmd_line[offset + i] = '\0';
    append_history(cmd_line);

    /* --- Command dispatch --- */
    if (str_eq(cmd, "help")) {
        append_history("HELP ABOUT VERSION");
        append_history("CLEAR MEM HEAP FILES");
        append_history("APPS  RUN <name> REBOOT");
        append_history("PAGING SYSCALLS PROC");

    } else if (str_eq(cmd, "paging")) {
        if (is_paging_enabled()) {
            append_history("Paging: Enabled");
            char dir_str[40];
            str_copy(dir_str, "CR3: 0x", 40);
            int p = str_len(dir_str);
            uint32_t cr3 = get_page_directory_addr();
            char hex[9];
            for (int j = 7; j >= 0; j--) {
                uint32_t val = (cr3 >> (j * 4)) & 0xF;
                if (val < 10) hex[7 - j] = '0' + val;
                else hex[7 - j] = 'A' + (val - 10);
            }
            hex[8] = '\0';
            for (int j = 0; hex[j] && p < 39; j++) dir_str[p++] = hex[j];
            dir_str[p] = '\0';
            append_history(dir_str);
            append_history("Kernel Map: 0-4MB (Priv)");
            append_history("User Map: 4-8MB (User)");
        } else {
            append_history("Paging: Disabled");
        }

    } else if (str_eq(cmd, "syscalls")) {
        append_history("Syscall Stats (int 0x80):");
        char line[40];
        char num[12];
        
        str_copy(line, "  sys_print:  ", 40);
        u32_to_dec(get_syscall_count(1), num);
        int p = str_len(line);
        for (int j = 0; num[j] && p < 39; j++) line[p++] = num[j];
        line[p] = '\0';
        append_history(line);

        str_copy(line, "  sys_malloc: ", 40);
        u32_to_dec(get_syscall_count(2), num);
        p = str_len(line);
        for (int j = 0; num[j] && p < 39; j++) line[p++] = num[j];
        line[p] = '\0';
        append_history(line);

        str_copy(line, "  sys_free:   ", 40);
        u32_to_dec(get_syscall_count(3), num);
        p = str_len(line);
        for (int j = 0; num[j] && p < 39; j++) line[p++] = num[j];
        line[p] = '\0';
        append_history(line);

        str_copy(line, "  sys_time:   ", 40);
        u32_to_dec(get_syscall_count(4), num);
        p = str_len(line);
        for (int j = 0; num[j] && p < 39; j++) line[p++] = num[j];
        line[p] = '\0';
        append_history(line);

        str_copy(line, "  sys_exit:   ", 40);
        u32_to_dec(get_syscall_count(5), num);
        p = str_len(line);
        for (int j = 0; num[j] && p < 39; j++) line[p++] = num[j];
        line[p] = '\0';
        append_history(line);

    } else if (str_eq(cmd, "proc")) {
        append_history("Processes:");
        int found = 0;
        for (int j = 0; j < MAX_PROCESSES; j++) {
            if (process_table[j].state != PROC_STATE_UNUSED) {
                found = 1;
                char line[40];
                char id_str[6];
                u32_to_dec(process_table[j].id, id_str);
                
                str_copy(line, id_str, 40);
                int p = str_len(line);
                while (p < 3) line[p++] = ' ';
                line[p] = '\0';
                
                int n_idx = 0;
                while (process_table[j].name[n_idx] && p < 15) {
                    line[p++] = process_table[j].name[n_idx++];
                }
                while (p < 16) line[p++] = ' ';
                line[p] = '\0';
                
                const char *st_str = "UNK";
                if (process_table[j].state == PROC_STATE_CREATED) st_str = "CREAT";
                else if (process_table[j].state == PROC_STATE_RUNNING) st_str = "RUN";
                else if (process_table[j].state == PROC_STATE_TERMINATED) st_str = "TERM";
                
                int s_idx = 0;
                while (st_str[s_idx] && p < 25) {
                    line[p++] = st_str[s_idx++];
                }
                while (p < 24) line[p++] = ' ';
                line[p] = '\0';
                
                char hex[9];
                uint32_t ent = process_table[j].entry_point;
                for (int k = 7; k >= 0; k--) {
                    uint32_t val = (ent >> (k * 4)) & 0xF;
                    if (val < 10) hex[7 - k] = '0' + val;
                    else hex[7 - k] = 'A' + (val - 10);
                }
                hex[8] = '\0';
                
                int h_idx = 0;
                while (hex[h_idx] && p < 39) {
                    line[p++] = hex[h_idx++];
                }
                line[p] = '\0';
                append_history(line);
            }
        }
        if (!found) {
            append_history("No processes in table");
        }

    } else if (str_eq(cmd, "about")) {
        append_history("SQ-OS by Harsh");
        append_history("Hybrid C+ASM kernel");

    } else if (str_eq(cmd, "version")) {
        append_history("SQ-OS v3.0");
        append_history("Kernel: hybrid-c-asm");

    } else if (str_eq(cmd, "clear")) {
        for (int j = 0; j < TERM_HISTORY_LINES; j++) {
            term_history[j][0] = '\0';
        }

    } else if (str_eq(cmd, "mem") || str_eq(cmd, "heap")) {
        /* Pull full heap statistics */
        HeapStats st;
        heap_stats(&st);

        /* Helper: build "Label: NNN B" into a history line */
        char ln[TERM_HISTORY_LEN];
        char num[12];

        /* Line 1: Used */
        str_copy(ln, "Used:  ", TERM_HISTORY_LEN);
        u32_to_dec(st.used_bytes, num);
        int p = str_len(ln);
        for (int j = 0; num[j] && p < TERM_HISTORY_LEN-3; j++) ln[p++]=num[j];
        ln[p++]='B'; ln[p]=0;
        append_history(ln);

        /* Line 2: Free */
        str_copy(ln, "Free:  ", TERM_HISTORY_LEN);
        u32_to_dec(st.free_bytes, num);
        p = str_len(ln);
        for (int j = 0; num[j] && p < TERM_HISTORY_LEN-3; j++) ln[p++]=num[j];
        ln[p++]='B'; ln[p]=0;
        append_history(ln);

        /* Line 3: Total */
        str_copy(ln, "Total: ", TERM_HISTORY_LEN);
        u32_to_dec(st.total_bytes, num);
        p = str_len(ln);
        for (int j = 0; num[j] && p < TERM_HISTORY_LEN-3; j++) ln[p++]=num[j];
        ln[p++]='B'; ln[p]=0;
        append_history(ln);

        /* Line 4: Blocks live / alloc / free */
        str_copy(ln, "Blk: ", TERM_HISTORY_LEN);
        u32_to_dec(st.block_count, num);
        p = str_len(ln);
        for (int j = 0; num[j] && p < TERM_HISTORY_LEN-12; j++) ln[p++]=num[j];
        /* append " A:N F:N" */
        ln[p++]=' '; ln[p++]='A'; ln[p++]=':';
        u32_to_dec(st.alloc_count, num);
        for (int j = 0; num[j] && p < TERM_HISTORY_LEN-6; j++) ln[p++]=num[j];
        ln[p++]=' '; ln[p++]='F'; ln[p++]=':';
        u32_to_dec(st.free_count, num);
        for (int j = 0; num[j] && p < TERM_HISTORY_LEN-2; j++) ln[p++]=num[j];
        ln[p]=0;
        append_history(ln);

    } else if (str_eq(cmd, "files")) {
        /* List root directory via FAT12 */
        append_history("Disk: reading...");
        if (fat12_init() != 0) {
            append_history("ERR: No FAT12 disk");
        } else {
            DirEntry entries[FAT12_MAX_FILES];
            int count = fat12_list_root(entries, FAT12_MAX_FILES);
            if (count == 0) {
                append_history("Root dir is empty");
            } else {
                for (int j = 0; j < count && j < 4; j++) {
                    /* Format: "NAME    .EXT" */
                    char line[TERM_HISTORY_LEN];
                    int p = 0;
                    for (int k = 0; k < 8 && entries[j].name[k] != ' '; k++) {
                        line[p++] = entries[j].name[k];
                    }
                    if (entries[j].ext[0] != ' ') {
                        line[p++] = '.';
                        for (int k = 0; k < 3 && entries[j].ext[k] != ' '; k++) {
                            line[p++] = entries[j].ext[k];
                        }
                    }
                    line[p] = '\0';
                    append_history(line);
                }
                if (count > 4) append_history("... (more files)");
            }
        }

    } else if (str_eq(cmd, "apps")) {
        /* List available programs from app directory */
        char names[8][13];
        int  count = loader_list_apps(names, 8);
        if (count <= 0) {
            append_history("No apps on disk");
            append_history("Run build.bat first");
        } else {
            append_history("Installed apps:");
            for (int j = 0; j < count; j++) {
                append_history(names[j]);
            }
        }

    } else if (str_starts_with(cmd, "run ")) {
        /* Load and execute program: run hello.app */
        const char *app_name = cmd + 4; /* skip "run " */
        if (app_name[0] == '\0') {
            append_history("Usage: run <name>");
            append_history("e.g.  run hello.app");
        } else {
            char out[APP_OUT_BUF_SIZE];
            append_history("Loading...");
            int result = load_program(app_name, out, APP_OUT_BUF_SIZE);
            if (result == 0) {
                /* Output might be multi-line — print up to 2 history lines */
                append_history(out);
                append_history("[OK]");
            } else if (result == -1) {
                append_history("ERR: App not found");
                append_history("Type APPS to list");
            } else if (result == -2) {
                append_history("ERR: Disk read fail");
            } else if (result == -3) {
                append_history("ERR: Out of memory");
            }
        }

    } else if (str_eq(cmd, "reboot")) {
        append_history("Rebooting...");
        /* Trigger reboot via PS/2 controller */
        outb(0x64, 0xFE);
        while (1) { __asm__ volatile("hlt"); }

    } else if (cmd[0] == '\0') {
        /* Empty enter — print blank prompt line */

    } else {
        append_history("Unknown command");
        append_history("Type HELP");
    }
}

/* ============================================================
 * terminal_handle_key — process incoming keyboard scancodes
 *
 * Extended scancodes (arrow keys) have an 0xE0 prefix:
 *   0xE0 0x48  Up arrow
 *   0xE0 0x50  Down arrow
 * We detect these with a simple two-byte state machine.
 * ============================================================ */
static uint8_t extended_key_pending = 0; /* set when 0xE0 was last seen */

void terminal_handle_key(uint8_t scancode) {
    if (scancode & 0x80) {
        extended_key_pending = 0;
        return; /* key release, ignore */
    }

    /* Extended key prefix */
    if (scancode == 0xE0) {
        extended_key_pending = 1;
        return;
    }

    if (extended_key_pending) {
        extended_key_pending = 0;

        if (scancode == 0x48) {
            /* Up arrow — recall older command */
            int next = cmd_hist_idx + 1;
            if (next < cmd_hist_count) {
                cmd_hist_idx = next;
                str_copy(term_input, cmd_hist[cmd_hist_idx], TERM_INPUT_MAX + 1);
                term_input_len = (uint32_t)str_len(term_input);
            }
            return;
        }

        if (scancode == 0x50) {
            /* Down arrow — recall newer command */
            int next = cmd_hist_idx - 1;
            if (next >= 0) {
                cmd_hist_idx = next;
                str_copy(term_input, cmd_hist[cmd_hist_idx], TERM_INPUT_MAX + 1);
                term_input_len = (uint32_t)str_len(term_input);
            } else {
                /* Past the newest entry — clear input */
                cmd_hist_idx = -1;
                term_input[0] = '\0';
                term_input_len = 0;
            }
            return;
        }

        return; /* Unknown extended key */
    }

    if (scancode >= 128) return;
    char c = scancode_map[scancode];
    if (c == 0) return;

    if (c == '\b') {
        /* Backspace */
        if (term_input_len > 0) {
            term_input_len--;
            term_input[term_input_len] = '\0';
        }
        cmd_hist_idx = -1; /* Cancel history browsing on edit */

    } else if (c == '\n') {
        /* Execute */
        term_input[term_input_len] = '\0';
        push_cmd_hist(term_input);        /* Save to recall history */
        terminal_execute_command(term_input);
        term_input_len = 0;
        term_input[0]  = '\0';

    } else {
        /* Normal character */
        if (term_input_len < TERM_INPUT_MAX) {
            term_input[term_input_len]     = c;
            term_input[term_input_len + 1] = '\0';
            term_input_len++;
        }
        cmd_hist_idx = -1; /* Cancel history browsing on new input */
    }
}

/* ============================================================
 * draw_terminal_content — render the terminal window
 *
 * Layout (window is approx 200×130):
 *
 *  ┌──────────────────────────────┐  ← win->y
 *  │ [TITLE BAR]                  │  ← 12px
 *  │ ─────────────────────────── │
 *  │  history[0]                  │  ← y+16
 *  │  history[1]                  │  ← y+28
 *  │  ...                         │
 *  │  history[7]                  │  ← y+16+7*12 = y+100
 *  │ ─────────────────────────── │
 *  │  SQ> input_  (prompt)        │  ← y+112
 *  └──────────────────────────────┘
 * ============================================================ */
void draw_terminal_content(Window *win) {
    /* 1. Black background */
    draw_rect(win->x + 2, win->y + 13, win->w - 4, win->h - 15, 0);

    /* 2. Output history lines — green on black */
    int min_x = win->x + 2;
    int max_x = win->x + win->w - 2;
    int min_y = win->y + 13;
    int max_y = win->y + win->h - 2;

    for (int i = 0; i < TERM_HISTORY_LINES; i++) {
        if (term_history[i][0] != '\0') {
            draw_text_clipped(win->x + 6, win->y + 16 + (i * 12), term_history[i], 10,
                              min_x, max_x, min_y, max_y);
        }
    }

    /* 3. Separator line above prompt */
    int sep_y = win->y + 16 + TERM_HISTORY_LINES * 12;
    draw_rect(win->x + 2, sep_y, win->w - 4, 1, 8); /* dark gray line */

    /* 4. Active prompt line with blinking block cursor */
    cursor_blink_tick++;

    char prompt_line[TERM_INPUT_MAX + 8];
    str_copy(prompt_line, "SQ> ", TERM_INPUT_MAX + 8);
    int base = str_len("SQ> ");
    for (int j = 0; j < (int)term_input_len && base + j < TERM_INPUT_MAX + 6; j++) {
        prompt_line[base + j] = term_input[j];
    }
    int cursor_pos = base + (int)term_input_len;

    /* Blink: show '_' for ~30 frames, hide for ~30 frames */
    if ((cursor_blink_tick & 63) < 40) {
        prompt_line[cursor_pos]     = '_';
        prompt_line[cursor_pos + 1] = '\0';
    } else {
        prompt_line[cursor_pos] = '\0';
    }

    draw_text_clipped(win->x + 6, sep_y + 3, prompt_line, 10,
                      min_x, max_x, min_y, max_y); /* bright green */
}
