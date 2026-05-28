#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

// Port I/O helper functions
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" :: "a"(value), "Nd"(port));
}

// Global kernel states / variable references
extern volatile uint32_t cur_row;
extern volatile uint32_t cur_col;
extern volatile uint32_t buf_len;

// Text mode screen functions
void clear_screen(void);
void draw_header(void);
void print_prompt(void);
void put_char(char c, uint8_t attr);
void print_str(const char *str, uint8_t attr);
void scroll_screen(void);

// Keyboard handling
void keyboard_handler(uint8_t scancode);

// VGA state transition functions
void init_vga_mode13(void);
void restore_text_mode(void);

// C kernel entry
void kernel_main(void);

#endif // KERNEL_H
