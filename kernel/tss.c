/* ================================================================
 * kernel/tss.c — Task State Segment initialisation
 *
 * Responsibilities:
 *   - Zero-initialise the TSS
 *   - Set ss0 = 0x10 (kernel data segment)
 *   - Set esp0 = 0x90000 (system task kernel stack, default)
 *   - Install TSS descriptor into GDT entry 5 via gdt_set_tss_entry()
 *   - Load the Task Register (TR) with selector 0x28 via tss_flush()
 *
 * tss_set_kernel_stack(stack_top) is called by the scheduler after
 * every context switch to keep esp0 pointing to the correct kernel
 * stack for the new current_process.
 * ================================================================ */

#include "tss.h"
#include "gdt.h"
#include <stdint.h>

/* Global TSS instance — referenced by GDT entry 5 */
TSS kernel_tss;

void tss_init(void)
{
    /* Zero the entire TSS structure */
    uint8_t *p = (uint8_t *)&kernel_tss;
    for (uint32_t i = 0; i < sizeof(TSS); i++) {
        p[i] = 0;
    }

    kernel_tss.ss0        = SEG_KDATA;          /* Kernel data segment 0x10 */
    kernel_tss.esp0       = 0x90000;            /* Default system-task stack */
    kernel_tss.iomap_base = (uint16_t)sizeof(TSS); /* No I/O permission map */

    /* Install TSS descriptor in GDT entry 5 */
    uint32_t base  = (uint32_t)&kernel_tss;
    uint32_t limit = (uint32_t)(sizeof(TSS) - 1);
    gdt_set_tss_entry(base, limit);

    /* Load TR register with TSS selector (0x28) */
    tss_flush();
}

/* Called by scheduler on every context switch */
void tss_set_kernel_stack(uint32_t stack_top)
{
    kernel_tss.esp0 = stack_top;
}
