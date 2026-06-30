#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* ================================================================
 * GDT Segment Selectors
 *
 * Selector = (index * 8) | RPL
 *
 *  0x00 — Null descriptor
 *  0x08 — Kernel Code  (DPL = 0)
 *  0x10 — Kernel Data  (DPL = 0)
 *  0x18 — User Code    (DPL = 3, raw selector without RPL)
 *  0x1B — User Code    (DPL = 3, selector with RPL = 3)
 *  0x20 — User Data    (DPL = 3, raw selector without RPL)
 *  0x23 — User Data    (DPL = 3, selector with RPL = 3)
 *  0x28 — TSS          (DPL = 0)
 * ================================================================ */
#define SEG_KCODE  0x08
#define SEG_KDATA  0x10
#define SEG_UCODE  0x1B   /* 0x18 | RPL=3 — load into CS for Ring 3 */
#define SEG_UDATA  0x23   /* 0x20 | RPL=3 — load into DS/SS for Ring 3 */
#define SEG_TSS    0x28

void gdt_init(void);
void gdt_set_tss_entry(uint32_t base, uint32_t limit);

/* Implemented in entry.asm — reloads GDT and all segment registers */
extern void gdt_flush(uint32_t gdt_ptr);

#endif /* GDT_H */
