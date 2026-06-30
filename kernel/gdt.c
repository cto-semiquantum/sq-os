/* ================================================================
 * kernel/gdt.c — Global Descriptor Table (6 entries)
 *
 * Entry layout:
 *   0 — Null
 *   1 — Kernel Code  (0x08)  DPL=0
 *   2 — Kernel Data  (0x10)  DPL=0
 *   3 — User Code    (0x1B)  DPL=3
 *   4 — User Data    (0x23)  DPL=3
 *   5 — TSS          (0x28)  DPL=0  (filled by tss_init)
 * ================================================================ */

#include "gdt.h"
#include <stdint.h>

/* ── Internal types ─────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;   /* bits  0-15 of limit  */
    uint16_t base_low;    /* bits  0-15 of base   */
    uint8_t  base_mid;    /* bits 16-23 of base   */
    uint8_t  access;      /* access byte          */
    uint8_t  gran;        /* flags [7:4] + limit high [3:0] */
    uint8_t  base_high;   /* bits 24-31 of base   */
} GDTEntry;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} GDTPtr;

/* ── Static GDT array + pointer ─────────────────────────────── */
static GDTEntry gdt[6];
static GDTPtr   gdt_ptr;

/* ── Helper: write one GDT entry ───────────────────────────── */
static void set_entry(int i, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t gran)
{
    gdt[i].base_low  = base & 0xFFFF;
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].gran      = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access    = access;
}

/* ── gdt_init — build and load the 6-entry GDT ─────────────── */
void gdt_init(void)
{
    /* Access byte encoding:
     *   bit7  = Present
     *   bit6-5= DPL (privilege level)
     *   bit4  = Descriptor type (1 = code/data)
     *   bit3  = Executable
     *   bit2  = Direction/Conforming
     *   bit1  = Read/Write
     *   bit0  = Accessed
     *
     * Granularity nibble (upper byte):
     *   bit7 = Granularity (1 = 4KB pages)
     *   bit6 = Size (1 = 32-bit)
     *   bit5 = 0
     *   bit4 = 0
     *   bits3-0 = Limit [19:16]
     *
     * 0xCF = 1100 | 1111 → 4KB gran, 32-bit, limit high = 0xF
     */

    set_entry(0, 0, 0, 0, 0);                 /* Null descriptor            */
    set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);     /* Kernel Code  DPL=0  0x08  */
    set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);     /* Kernel Data  DPL=0  0x10  */
    set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF);     /* User Code    DPL=3  0x18  */
    set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF);     /* User Data    DPL=3  0x20  */
    set_entry(5, 0, 0, 0, 0);                 /* TSS (filled by tss_init)   */

    gdt_ptr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdt_ptr.base  = (uint32_t)gdt;

    gdt_flush((uint32_t)&gdt_ptr);
}

/* ── gdt_set_tss_entry — write TSS base/limit into entry 5 ── */
void gdt_set_tss_entry(uint32_t base, uint32_t limit)
{
    /* Access: 0x89 = Present, DPL=0, 32-bit TSS (Available) */
    set_entry(5, base, limit, 0x89, 0x00);
}
