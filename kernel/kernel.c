#include "kernel.h"
#include "desktop.h"
#include "graphics.h"
#include "memory.h"
#include "paging.h"
#include "gdt.h"
#include "tss.h"
#include "process.h"
#include "syscall.h"
#include "../net/net.h"

#define VIDEO_MEM ((volatile uint16_t *)0xB8000)
#define MAX_ROWS 25
#define MAX_COLS 80

volatile uint32_t cur_row = 2;
volatile uint32_t cur_col = 0;
volatile uint32_t buf_len = 0;

volatile uint32_t system_ticks = 0;

void timer_handler(void) {
    system_ticks++;
}

static char input_buffer[80];

static const char *title_str   = "  SQ-OS  |  32-BIT PROTECTED MODE SHELL  ";
static const char *prompt_str  = "SQ> ";
static const char *err_str     = "Unknown command. Type help.";
static const char *help_str    = "Commands: help  clear  about  version  reboot  gui";
static const char *about_str   = "SQ-OS | 32-bit Protected Mode | Hybrid C+ASM Kernel | by Harsh";
static const char *version_str = "SQ-OS v2.0 | Kernel: hybrid-c-asm | Arch: x86 PM | Build: 2026";

// VGA Mode 13h registers tables
static const uint8_t crtc13h[25 * 2] = {
    0x00,0x5F, 0x01,0x4F, 0x02,0x50, 0x03,0x82, 0x04,0x54,
    0x05,0x80, 0x06,0xBF, 0x07,0x1F, 0x08,0x00, 0x09,0x41,
    0x0A,0x00, 0x0B,0x00, 0x0C,0x00, 0x0D,0x00, 0x10,0x9C,
    0x11,0x8E, 0x12,0x8F, 0x13,0x28, 0x14,0x40, 0x15,0x96,
    0x16,0xB9, 0x17,0xA3, 0x18,0xFF, 0x0E,0x00, 0x0F,0x00
};

static const uint8_t gc13h[9 * 2] = {
    0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00,
    0x05,0x40, 0x06,0x05, 0x07,0x0F, 0x08,0xFF
};

static const uint8_t ac13h[21 * 2] = {
    0x00,0x00, 0x01,0x01, 0x02,0x02, 0x03,0x03, 0x04,0x04,
    0x05,0x05, 0x06,0x06, 0x07,0x07, 0x08,0x08, 0x09,0x09,
    0x0A,0x0A, 0x0B,0x0B, 0x0C,0x0C, 0x0D,0x0D, 0x0E,0x0E,
    0x0F,0x0F, 0x10,0x41, 0x11,0x00, 0x12,0x0F, 0x13,0x00,
    0x14,0x00
};

static const uint8_t gctext[9 * 2] = {
    0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00,
    0x05,0x10, 0x06,0x0E, 0x07,0x00, 0x08,0xFF
};

// Keyboard scancodes mapping to ASCII
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

// Text mode screen output logic
void clear_screen(void) {
    // Fill text rows 2-24 with dark blue background spaces (0x0120)
    for (int i = 80 * 2; i < 80 * 25; i++) {
        VIDEO_MEM[i] = 0x0120;
    }
}

void draw_header(void) {
    // Row 0: cyan bg (0xB0), space character (0x20)
    for (int i = 0; i < 80; i++) {
        VIDEO_MEM[i] = 0xB020;
    }
    // Print title centered on row 0
    int start = (80 - 41) / 2; // title length is 41 chars
    for (int i = 0; title_str[i] != '\0'; i++) {
        VIDEO_MEM[start + i] = (0xB0 << 8) | title_str[i];
    }
    // Row 1: gray bg (0x08), space character (0x20)
    for (int i = 80; i < 160; i++) {
        VIDEO_MEM[i] = 0x0820;
    }
}

void scroll_screen(void) {
    // Copy rows 3..24 to rows 2..23
    for (int r = 2; r < 24; r++) {
        for (int c = 0; c < 80; c++) {
            VIDEO_MEM[r * 80 + c] = VIDEO_MEM[(r + 1) * 80 + c];
        }
    }
    // Clear last row
    for (int c = 0; c < 80; c++) {
        VIDEO_MEM[24 * 80 + c] = 0x0120;
    }
    cur_row = 23;
}

void put_char(char c, uint8_t attr) {
    if (c == '\n') {
        cur_col = 0;
        cur_row++;
        if (cur_row >= MAX_ROWS) {
            scroll_screen();
        }
        return;
    }
    if (c == '\b') {
        if (cur_col > 0) {
            cur_col--;
            VIDEO_MEM[cur_row * 80 + cur_col] = (attr << 8) | ' ';
        }
        return;
    }

    VIDEO_MEM[cur_row * 80 + cur_col] = (attr << 8) | c;
    cur_col++;
    if (cur_col >= MAX_COLS) {
        cur_col = 0;
        cur_row++;
        if (cur_row >= MAX_ROWS) {
            scroll_screen();
        }
    }
}

void print_str(const char *str, uint8_t attr) {
    while (*str) {
        put_char(*str, attr);
        str++;
    }
}

void print_prompt(void) {
    print_str(prompt_str, 0x0B); // Light cyan
}

void dispatch_command(void) {
    print_str("\n", 0x07);

    if (strcmp(input_buffer, "help") == 0) {
        print_str(help_str, 0x0A); // Light green
    } else if (strcmp(input_buffer, "clear") == 0) {
        clear_screen();
        draw_header();
        cur_row = 2;
        cur_col = 0;
        return; // Skip prompt printing here
    } else if (strcmp(input_buffer, "about") == 0) {
        print_str(about_str, 0x0B); // Light cyan
    } else if (strcmp(input_buffer, "version") == 0) {
        print_str(version_str, 0x0F); // White
    } else if (strcmp(input_buffer, "reboot") == 0) {
        outb(0x64, 0xFE); // Trigger system reboot via 8042
        while (1) {
            __asm__ volatile("hlt");
        }
    } else if (strcmp(input_buffer, "gui") == 0) {
        // Enters Mode 13h register config
        init_vga_mode13();
        run_gui_loop();
        // Restores text mode 3
        restore_text_mode();
        clear_screen();
        draw_header();
        cur_row = 2;
        cur_col = 0;
        return;
    } else if (buf_len > 0) {
        print_str(err_str, 0x0C); // Light red
    }

    print_str("\n", 0x07);
    print_prompt();
}

void keyboard_handler(uint8_t scancode) {
    if (scancode & 0x80) {
        // Key release event, ignore
        return;
    }

    if (scancode >= 128) return;

    char c = scancode_map[scancode];
    if (c == 0) return;

    if (c == '\b') {
        if (buf_len > 0) {
            buf_len--;
            input_buffer[buf_len] = '\0';
            put_char('\b', 0x0F);
        }
    } else if (c == '\n') {
        input_buffer[buf_len] = '\0';
        dispatch_command();
        buf_len = 0;
        input_buffer[0] = '\0';
    } else {
        if (buf_len < 79) {
            input_buffer[buf_len] = c;
            buf_len++;
            input_buffer[buf_len] = '\0';
            put_char(c, 0x0F);
        }
    }
}

// PS/2 Controller Helper
static void ps2_wait_write(void) {
    while (inb(0x64) & 0x02);
}

// VGA Transition Logic
void init_vga_mode13(void) {
    __asm__ volatile("cli");

    // Misc Output
    outb(0x3C2, 0x63);

    // Sequencer
    outb(0x3C4, 0x00); outb(0x3C5, 0x03); // Reset
    outb(0x3C4, 0x01); outb(0x3C5, 0x01); // Clocking Mode
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F); // Map Mask
    outb(0x3C4, 0x03); outb(0x3C5, 0x00); // Character Map Select
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E); // Memory Mode

    // Unlock CRTC registers
    outb(0x3D4, 0x11);
    outb(0x3D5, 0x8E);

    // CRTC Registers
    for (int i = 0; i < 25; i++) {
        outb(0x3D4, crtc13h[i * 2]);
        outb(0x3D5, crtc13h[i * 2 + 1]);
    }

    // Graphics Controller Registers
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, gc13h[i * 2]);
        outb(0x3CF, gc13h[i * 2 + 1]);
    }

    // Attribute Controller Registers
    inb(0x3DA); // Reset flip-flop
    for (int i = 0; i < 21; i++) {
        outb(0x3C0, ac13h[i * 2]);
        outb(0x3C0, ac13h[i * 2 + 1]);
    }
    outb(0x3C0, 0x20); // Enable palette

    // Initialize PS/2 Mouse
    // Drain auxiliary output buffer
    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    // Enable auxiliary mouse device
    ps2_wait_write();
    outb(0x64, 0xA8);

    // Enable IRQ12 in command byte
    ps2_wait_write();
    outb(0x64, 0x20);
    
    uint8_t ccb = 0;
    for (int timeout = 0; timeout < 65535; timeout++) {
        if (inb(0x64) & 0x01) {
            ccb = inb(0x60);
            break;
        }
    }
    ccb |= 0x02;  // enable IRQ12 for mouse
    ccb &= 0xDF;  // enable mouse clock (clear bit 5)
    
    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, ccb);

    // Tell mouse to use default settings
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, 0xF6);
    
    // Drain ACK
    for (int timeout = 0; timeout < 65535; timeout++) {
        if (inb(0x64) & 0x01) {
            inb(0x60);
            break;
        }
    }

    // Enable mouse packet streaming
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, 0xF4);
    
    // Drain ACK
    for (int timeout = 0; timeout < 65535; timeout++) {
        if (inb(0x64) & 0x01) {
            inb(0x60);
            break;
        }
    }

    // Mask keyboard (IRQ1) and mouse (IRQ12) interrupts on the PIC,
    // so they do not trigger the CPU interrupt handlers (preserving polling in the GUI loop),
    // but keep other interrupts (such as the timer IRQ0) unmasked.
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask = inb(0xA1);
    outb(0x21, master_mask | 0x02); // Mask IRQ1 (keyboard)
    outb(0xA1, slave_mask | 0x10);  // Mask IRQ12 (mouse)

    // Enable CPU interrupts (timer IRQ0 will now fire in the background)
    __asm__ volatile("sti");
}

void restore_text_mode(void) {
    __asm__ volatile("cli");

    // Unmask keyboard (IRQ1) and mouse (IRQ12) interrupts on the PIC
    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask = inb(0xA1);
    outb(0x21, master_mask & ~0x02); // Unmask IRQ1 (keyboard)
    outb(0xA1, slave_mask & ~0x10);  // Unmask IRQ12 (mouse)

    // Misc Output
    outb(0x3C2, 0x67);

    // Sequencer
    outb(0x3C4, 0x00); outb(0x3C5, 0x03); // Reset
    outb(0x3C4, 0x01); outb(0x3C5, 0x00); // Clocking Mode
    outb(0x3C4, 0x02); outb(0x3C5, 0x03); // Map Mask
    outb(0x3C4, 0x03); outb(0x3C5, 0x00); // Character Map Select
    outb(0x3C4, 0x04); outb(0x3C5, 0x02); // Memory Mode

    // Graphics Controller text mode settings
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, gctext[i * 2]);
        outb(0x3CF, gctext[i * 2 + 1]);
    }

    __asm__ volatile("sti");
}

void draw_splash_frame(int percent) {
    // Clear backbuffer to black
    clear_backbuffer();

    // Centered retro dialog card (light gray body, 240x140 at (40, 20))
    int card_x = 40;
    int card_y = 20;
    int card_w = 240;
    int card_h = 140;
    draw_rect(card_x, card_y, card_w, card_h, 7);

    // Bevel highlights & shadows
    draw_rect(card_x, card_y, card_w, 1, 15);       // Top white border
    draw_rect(card_x, card_y, 1, card_h, 15);       // Left white border
    draw_rect(card_x, card_y + card_h - 1, card_w, 1, 8); // Bottom dark gray shadow
    draw_rect(card_x + card_w - 1, card_y, 1, card_h, 8); // Right dark gray shadow

    // Logo title
    draw_text(90, 45, "SEMIQUANTUM OS", 1);          // Classic Navy Blue text
    draw_text(85, 60, "Classic Edition", 8);         // Dark gray subtitle

    // Loading bar frame (dark gray border) at (60, 95), size 200x12
    int bar_x = 60;
    int bar_y = 95;
    int bar_w = 200;
    int bar_h = 12;
    draw_rect(bar_x, bar_y, bar_w, bar_h, 8);
    draw_rect(bar_x + 1, bar_y + 1, bar_w - 2, bar_h - 2, 0); // Black background

    // Fill loading bar (cyan progress color 11)
    int fill_w = (percent * (bar_w - 4)) / 100;
    if (fill_w > 0) {
        draw_rect(bar_x + 2, bar_y + 2, fill_w, bar_h - 4, 11);
    }

    // Loading status text below progress bar
    char pct_str[24];
    pct_str[0] = 'B'; pct_str[1] = 'o'; pct_str[2] = 'o'; pct_str[3] = 't';
    pct_str[4] = 'i'; pct_str[5] = 'n'; pct_str[6] = 'g'; pct_str[7] = '.';
    pct_str[8] = '.'; pct_str[9] = '.'; pct_str[10] = ' ';
    if (percent == 100) {
        pct_str[11] = '1'; pct_str[12] = '0'; pct_str[13] = '0'; pct_str[14] = '%'; pct_str[15] = '\0';
    } else {
        pct_str[11] = '0' + (percent / 10);
        pct_str[12] = '0' + (percent % 10);
        pct_str[13] = '%'; pct_str[14] = '\0';
    }
    draw_text(110, 120, pct_str, 0); // Black status text

    // Copy to VGA screen (Vsync-aligned)
    swap_buffers();
}

void kernel_main(void) {
    // 0. Initialise kernel heap (must be first)
    heap_init();

    // Initialise virtual memory paging (maps kernel 4MB and user 4MB)
    paging_init();

    // 0a. Load 6-entry GDT with Ring 3 (user) code and data segments
    gdt_init();

    // 0b. Initialise TSS and load TR register (needed for Ring 3 interrupts)
    tss_init();

    // Initialise process control block system
    process_init();

    // Initialise system call routing
    syscall_init();

    // Initialise networking stack
    net_init();

    // 1. Immediately transition to VGA graphics mode 13h
    init_vga_mode13();

    // 2. Perform smooth loading boot splash sequence
    for (int p = 0; p <= 100; p++) {
        draw_splash_frame(p);
        // Delay approximately 15ms per percentage update
        for (volatile int delay = 0; delay < 0x2C000; delay++);
    }

    // 3. Transition directly to GUI desktop environment loop
    run_gui_loop();

    // 4. Safe recovery fallback to text command shell if GUI loop exits (ESC pressed)
    restore_text_mode();
    clear_screen();
    draw_header();
    print_prompt();

    // Re-enable interrupts for keyboard text input
    __asm__ volatile("sti");

    // Idle loop
    while (1) {
        net_poll();
        __asm__ volatile("hlt");
    }
}
