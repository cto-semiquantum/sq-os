#ifndef TSS_H
#define TSS_H

#include <stdint.h>

/* ================================================================
 * TSS — Task State Segment (104 bytes, x86 hardware format)
 *
 * Only esp0 and ss0 are used by SQ-OS.
 * When the CPU transitions from Ring 3 → Ring 0 on any interrupt,
 * it reads ss0:esp0 from the TSS and switches the stack there.
 *
 * esp0 must be updated on every process context switch to point
 * to the TOP of the incoming process's kernel stack.
 * ================================================================ */
typedef struct __attribute__((packed)) {
    uint32_t prev_tss;   /* 0x00 - Previous TSS (not used)        */
    uint32_t esp0;       /* 0x04 - Kernel stack pointer (CRITICAL) */
    uint32_t ss0;        /* 0x08 - Kernel stack segment = 0x10    */
    uint32_t esp1;       /* 0x0C - Ring 1 stack (unused)          */
    uint32_t ss1;        /* 0x10 */
    uint32_t esp2;       /* 0x14 - Ring 2 stack (unused)          */
    uint32_t ss2;        /* 0x18 */
    uint32_t cr3;        /* 0x1C - Page directory (unused, flat)  */
    uint32_t eip;        /* 0x20 */
    uint32_t eflags;     /* 0x24 */
    uint32_t eax;        /* 0x28 */
    uint32_t ecx;        /* 0x2C */
    uint32_t edx;        /* 0x30 */
    uint32_t ebx;        /* 0x34 */
    uint32_t esp;        /* 0x38 */
    uint32_t ebp;        /* 0x3C */
    uint32_t esi;        /* 0x40 */
    uint32_t edi;        /* 0x44 */
    uint32_t es;         /* 0x48 */
    uint32_t cs;         /* 0x4C */
    uint32_t ss;         /* 0x50 */
    uint32_t ds;         /* 0x54 */
    uint32_t fs;         /* 0x58 */
    uint32_t gs;         /* 0x5C */
    uint32_t ldt;        /* 0x60 */
    uint16_t trap;       /* 0x64 */
    uint16_t iomap_base; /* 0x66 - I/O permission map base        */
} TSS;

extern TSS kernel_tss;

void tss_init(void);
void tss_set_kernel_stack(uint32_t stack_top);

/* Implemented in entry.asm — executes: ltr 0x28 */
extern void tss_flush(void);

#endif /* TSS_H */
