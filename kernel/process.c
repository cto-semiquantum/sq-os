/* ================================================================
 * kernel/process.c — Process subsystem
 *
 * Features:
 *   - Preemptive priority-based Round-Robin scheduler
 *   - Ring 0 (kernel) and Ring 3 (user mode) process support
 *   - Dual stacks per Ring 3 process: kernel stack + user stack
 *   - Sleep queue (sys_sleep / sys_sleep_ms)
 *   - Window auto-close on process termination
 *   - Page fault crash handler (terminates Ring 3 process safely)
 *   - TSS esp0 update on every context switch
 * ================================================================ */

#include "process.h"
#include "tss.h"
#include "gdt.h"
#include "terminal_app.h"
#include "memory.h"
#include "paging.h"
#include "window_manager.h"

Process  process_table[MAX_PROCESSES];
Process *current_process = (void *)0;
static uint32_t next_pid = 1;

/* ── process_init ──────────────────────────────────────────────
 * Register Process 0 as the main GUI/system task (Ring 0, HIGH)
 * ─────────────────────────────────────────────────────────────*/
void process_init(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state           = PROC_STATE_UNUSED;
        process_table[i].id              = 0;
        process_table[i].name[0]         = '\0';
        process_table[i].entry_point     = 0;
        process_table[i].size            = 0;
        process_table[i].esp             = 0;
        process_table[i].stack_mem       = (void *)0;
        process_table[i].kernel_stack_top= 0;
        process_table[i].user_stack_mem  = (void *)0;
        process_table[i].user_esp        = 0;
        process_table[i].ring            = 0;
        process_table[i].priority        = PRIO_NORMAL;
        process_table[i].sleep_ticks     = 0;
        process_table[i].cr3             = 0;
        process_table[i].alloced_count   = 0;
        process_table[i].user_heap_ptr   = 0;
        for (int k = 0; k < 32; k++) {
            process_table[i].alloced_blocks[k] = (void *)0;
        }
    }

    /* Process 0: GUI / system — always Ring 0, PRIO_HIGH */
    Process *p = &process_table[0];
    p->id              = 0;
    p->state           = PROC_STATE_RUNNING;
    p->ring            = 0;
    p->priority        = PRIO_HIGH;
    p->sleep_ticks     = 0;
    p->stack_mem       = (void *)0;
    p->kernel_stack_top= 0x90000;   /* Fixed boot stack */
    p->user_stack_mem  = (void *)0;
    p->user_esp        = 0;
    p->entry_point     = 0;
    p->esp             = 0;
    p->cr3             = 0;
    p->alloced_count   = 0;
    p->user_heap_ptr   = 0;

    const char *sys_name = "system";
    int j = 0;
    while (sys_name[j]) { p->name[j] = sys_name[j]; j++; }
    p->name[j] = '\0';

    current_process = p;
    next_pid = 1;
}

/* ── process_create ────────────────────────────────────────────
 * Allocate and initialise a new process.
 *
 * ring == 0: kernel-mode process (single kernel stack)
 *   Kernel stack iret frame: EIP, CS=0x08, EFLAGS
 *
 * ring == 3: user-mode process (dual stacks)
 *   Kernel stack iret frame: EIP, CS=0x1B, EFLAGS, ESP3, SS=0x23
 * ─────────────────────────────────────────────────────────────*/
Process *process_create(const char *name, uint32_t entry_point,
                        uint32_t size, uint8_t ring,
                        uint32_t cr3, uint32_t user_stack_top,
                        uint32_t user_heap_start)
{

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_UNUSED) continue;

        Process *p = &process_table[i];
        p->id          = next_pid++;
        p->ring        = ring;
        p->priority    = PRIO_NORMAL;
        p->sleep_ticks = 0;
        p->entry_point = entry_point;
        p->size        = size;
        p->cr3         = cr3;
        p->user_heap_ptr = user_heap_start;
        p->alloced_count = 0;
        for (int k = 0; k < 32; k++) {
            p->alloced_blocks[k] = (void *)0;
        }

        /* Copy name */
        int j = 0;
        while (name[j] && j < 31) { p->name[j] = name[j]; j++; }
        p->name[j] = '\0';

        /* ── Allocate kernel stack (4 KB) ─────────────────── */
        p->stack_mem = (uint8_t *)kmalloc(4096);
        if (!p->stack_mem) {
            p->state = PROC_STATE_UNUSED;
            return (void *)0;
        }
        p->kernel_stack_top = (uint32_t)(p->stack_mem + 4096);

        /* ── Allocate user stack for Ring 3 (4 KB) ────────── */
        p->user_stack_mem = (void *)0;
        p->user_esp       = 0;

        uint32_t act_user_stack_top = 0;
        if (ring == 3) {
            if (cr3 != 0) {
                /* ELF binary with separate page tables: allocate and map stack to 0x007FF000 */
                p->user_stack_mem = (uint8_t *)kmalloc(4096 * 2);
                if (!p->user_stack_mem) {
                    kfree(p->stack_mem);
                    p->state = PROC_STATE_UNUSED;
                    return (void *)0;
                }
                p->alloced_blocks[p->alloced_count++] = p->user_stack_mem;
                uint32_t phys_stack = ((uint32_t)p->user_stack_mem + 4095) & ~4095;

                /* Zero the stack page */
                for (int k = 0; k < 1024; k++) {
                    ((uint32_t *)phys_stack)[k] = 0;
                }

                map_user_page((uint32_t *)cr3, 0x007FF000, phys_stack, p->alloced_blocks, &p->alloced_count);
                act_user_stack_top = 0x00800000;
                p->user_esp = act_user_stack_top;
            } else {
                /* Legacy flat binary: stack in kernel heap space */
                p->user_stack_mem = (uint8_t *)kmalloc(4096);
                if (!p->user_stack_mem) {
                    kfree(p->stack_mem);
                    p->state = PROC_STATE_UNUSED;
                    return (void *)0;
                }
                act_user_stack_top = (uint32_t)(p->user_stack_mem + 4096);
                p->user_esp = act_user_stack_top;
            }
        }

        /* ── Build dummy interrupt frame on kernel stack ───── */
        uint32_t *stack = (uint32_t *)p->kernel_stack_top;

        if (ring == 3) {
            /* 5-word iret frame for privilege change Ring 0 → Ring 3 */
            *(--stack) = SEG_UDATA;             /* SS3  (User Data | RPL=3) */
            *(--stack) = act_user_stack_top;    /* ESP3 (top of user stack) */
            *(--stack) = 0x0202;                /* EFLAGS (IF=1)            */
            *(--stack) = SEG_UCODE;             /* CS   (User Code | RPL=3) */
            *(--stack) = entry_point;           /* EIP  (Ring 3 entry point)*/
        } else {
            /* 3-word iret frame — stays in Ring 0, no stack switch */
            *(--stack) = 0x0202;                /* EFLAGS (IF=1) */
            *(--stack) = SEG_KCODE;             /* CS (Kernel code 0x08)    */
            *(--stack) = entry_point;           /* EIP                      */
        }

        /* pushad register frame (all zeroes — popad restores these) */
        *(--stack) = 0;   /* EDI */
        *(--stack) = 0;   /* ESI */
        *(--stack) = 0;   /* EBP */
        *(--stack) = 0;   /* ESP (ignored by popad) */
        *(--stack) = 0;   /* EBX */
        *(--stack) = 0;   /* EDX */
        *(--stack) = 0;   /* ECX */
        *(--stack) = 0;   /* EAX */

        p->esp   = (uint32_t)stack;
        p->state = PROC_STATE_READY;
        return p;
    }
    return (void *)0; /* Process table full */
}

/* ── process_exit ──────────────────────────────────────────── */
void process_exit(int code)
{
    (void)code;
    if (current_process) {
        current_process->state = PROC_STATE_TERMINATED;

        char exit_msg[48];
        int len = 0;
        while (current_process->name[len] && len < 20)
            exit_msg[len] = current_process->name[len++];
        const char *suf = " exited";
        int s = 0;
        while (suf[s] && len < 47) exit_msg[len++] = suf[s++];
        exit_msg[len] = '\0';
        append_history(exit_msg);

        __asm__ volatile("int $32");
        while (1) { __asm__ volatile("hlt"); }
    }
}

/* ── sys_sleep ─────────────────────────────────────────────── */
void sys_sleep(uint32_t ticks)
{
    if (current_process && ticks > 0) {
        current_process->sleep_ticks = ticks;
        current_process->state       = PROC_STATE_SLEEPING;
        __asm__ volatile("int $32");
    }
}

void sys_sleep_ms(uint32_t ms)
{
    uint32_t ticks = ms / 55;
    if (ticks == 0 && ms > 0) ticks = 1;
    sys_sleep(ticks);
}

/* ── update_tss_esp0 ───────────────────────────────────────────
 * Called from timer_isr (entry.asm) after every context switch.
 * Updates TSS.esp0 so that the next Ring 3 → Ring 0 interrupt
 * finds the correct kernel stack for current_process.
 * ─────────────────────────────────────────────────────────────*/
void update_tss_esp0(void)
{
    if (current_process) {
        tss_set_kernel_stack(current_process->kernel_stack_top);
    }
}

/* ── page_fault_handler ────────────────────────────────────────
 * Called from page_fault_isr in entry.asm.
 *
 * error_code bits:
 *   bit 0 — Present (0 = not-present page, 1 = protection violation)
 *   bit 1 — Write   (0 = read, 1 = write)
 *   bit 2 — User    (0 = supervisor, 1 = user-mode access)
 *
 * Returns:
 *   0 — Ring 3 process was terminated; caller must reschedule.
 *   1 — Kernel fault; caller halts.
 * ─────────────────────────────────────────────────────────────*/
int page_fault_handler(uint32_t error_code, uint32_t fault_addr)
{
    /* Determine whether fault came from Ring 3 */
    int from_user = (error_code & 0x4) ? 1 : 0;

    if (from_user || (current_process && current_process->ring == 3)) {
        /* ── Ring 3 crash — terminate and reschedule ───────── */
        if (current_process) {
            current_process->state = PROC_STATE_TERMINATED;

            char msg[64];
            int len = 0;

            /* Print: "CRASH: <name> [Segfault @0x...]" */
            const char *pre = "CRASH: ";
            for (int i = 0; pre[i] && len < 63; i++) msg[len++] = pre[i];
            for (int i = 0; current_process->name[i] && len < 50; i++)
                msg[len++] = current_process->name[i];
            const char *suf = " Segfault";
            for (int i = 0; suf[i] && len < 63; i++) msg[len++] = suf[i];
            msg[len] = '\0';
            append_history(msg);

            /* Print fault address as hex for debugging */
            char addr_msg[32];
            addr_msg[0] = 'A'; addr_msg[1] = 'd'; addr_msg[2] = 'd';
            addr_msg[3] = 'r'; addr_msg[4] = ':'; addr_msg[5] = '0';
            addr_msg[6] = 'x';
            for (int k = 7; k >= 0; k--) {
                uint32_t nib = (fault_addr >> (k * 4)) & 0xF;
                addr_msg[7 + (7 - k)] = (nib < 10) ? ('0' + nib) : ('A' + nib - 10);
            }
            addr_msg[15] = '\0';
            append_history(addr_msg);
        }
        return 0; /* Tell page_fault_isr to reschedule */
    }

    /* ── Ring 0 kernel fault — panic ───────────────────────── */
    append_history("KERNEL PANIC: Page Fault in Ring 0!");
    return 1; /* Tell page_fault_isr to halt */
}

/* ── scheduler_switch ──────────────────────────────────────────
 * Called from timer_isr with the current kernel ESP.
 * Returns the ESP of the next process to run.
 * ─────────────────────────────────────────────────────────────*/
uint32_t scheduler_switch(uint32_t current_esp)
{
    /* 1. Save ESP of the currently running task */
    if (current_process) {
        current_process->esp = current_esp;
        if (current_process->state == PROC_STATE_RUNNING)
            current_process->state = PROC_STATE_READY;
    }

    /* 2. Clean up terminated processes */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROC_STATE_TERMINATED) continue;
        if (&process_table[i] == current_process) continue;

        /* Close any linked GUI windows */
        for (int w = 0; w < NUM_WINDOWS; w++) {
            if (window_order[w] && window_order[w]->pid == process_table[i].id) {
                window_order[w]->visible = 0;
                window_order[w]->active  = 0;
                window_order[w]->pid     = 0;
            }
        }

        /* Free kernel stack */
        if (process_table[i].stack_mem) {
            kfree(process_table[i].stack_mem);
            process_table[i].stack_mem = (void *)0;
        }
        /* Free user stack (Ring 3 only) */
        if (process_table[i].user_stack_mem) {
            kfree(process_table[i].user_stack_mem);
            process_table[i].user_stack_mem = (void *)0;
        }
        /* Free code buffer */
        if (process_table[i].entry_point && process_table[i].id != 0) {
            kfree((void *)process_table[i].entry_point);
            process_table[i].entry_point = 0;
        }
        /* Free other tracked dynamic memory allocations */
        for (int k = 0; k < process_table[i].alloced_count; k++) {
            if (process_table[i].alloced_blocks[k]) {
                kfree(process_table[i].alloced_blocks[k]);
                process_table[i].alloced_blocks[k] = (void *)0;
            }
        }
        process_table[i].alloced_count = 0;
        process_table[i].state = PROC_STATE_UNUSED;
    }

    /* 3. Tick sleeping processes and wake those whose timer expired */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_STATE_SLEEPING) {
            if (process_table[i].sleep_ticks > 0)
                process_table[i].sleep_ticks--;
            if (process_table[i].sleep_ticks == 0)
                process_table[i].state = PROC_STATE_READY;
        }
    }

    /* 4. Find the highest priority level with a runnable task */
    int max_prio = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        ProcessState s = process_table[i].state;
        if (s == PROC_STATE_READY || s == PROC_STATE_CREATED || s == PROC_STATE_RUNNING) {
            if ((int)process_table[i].priority > max_prio)
                max_prio = (int)process_table[i].priority;
        }
    }

    /* 5. Round-Robin within that highest priority level */
    int next_idx = -1;
    if (max_prio != -1) {
        int cur_idx = current_process ? (int)(current_process - process_table) : 0;
        for (int i = 1; i <= MAX_PROCESSES; i++) {
            int idx = (cur_idx + i) % MAX_PROCESSES;
            ProcessState s = process_table[idx].state;
            if ((s == PROC_STATE_READY || s == PROC_STATE_CREATED || s == PROC_STATE_RUNNING)
                && process_table[idx].priority == (uint32_t)max_prio) {
                next_idx = idx;
                break;
            }
        }
    }

    /* 6. Switch to new process or fall back to system */
    if (next_idx != -1) {
        current_process = &process_table[next_idx];
        current_process->state = PROC_STATE_RUNNING;
    } else {
        if (current_process &&
            (current_process->state == PROC_STATE_RUNNING ||
             current_process->state == PROC_STATE_READY)) {
            current_process->state = PROC_STATE_RUNNING;
        } else {
            current_process = &process_table[0];
            current_process->state = PROC_STATE_RUNNING;
        }
    }

    /* 7. Switch CR3 page directory if process has a custom one, otherwise use default kernel one */
    extern uint32_t get_page_directory_addr(void);
    if (current_process->cr3 != 0) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_process->cr3));
    } else {
        __asm__ volatile("mov %0, %%cr3" : : "r"(get_page_directory_addr()));
    }

    return current_process->esp;
}

Process *process_create_elf(Process *p, const char *name, uint32_t entry_point,
                            uint32_t size)
{

    p->id          = next_pid++;
    p->ring        = 3;
    p->priority    = PRIO_NORMAL;
    p->sleep_ticks = 0;
    p->entry_point = entry_point;
    p->size        = size;

    /* Copy name */
    int j = 0;
    while (name[j] && j < 31) { p->name[j] = name[j]; j++; }
    p->name[j] = '\0';

    /* Allocate kernel stack (4 KB) */
    p->stack_mem = (uint8_t *)kmalloc(4096);
    if (!p->stack_mem) {
        p->state = PROC_STATE_UNUSED;
        return (void *)0;
    }
    p->kernel_stack_top = (uint32_t)(p->stack_mem + 4096);

    /* Allocate user stack and map to 0x007FF000 */
    p->user_stack_mem = (uint8_t *)kmalloc(4096 * 2);
    if (!p->user_stack_mem) {
        kfree(p->stack_mem);
        p->state = PROC_STATE_UNUSED;
        return (void *)0;
    }
    p->alloced_blocks[p->alloced_count++] = p->user_stack_mem;
    uint32_t phys_stack = ((uint32_t)p->user_stack_mem + 4095) & ~4095;

    /* Zero the stack page */
    for (int k = 0; k < 1024; k++) {
        ((uint32_t *)phys_stack)[k] = 0;
    }

    map_user_page((uint32_t *)p->cr3, 0x007FF000, phys_stack, p->alloced_blocks, &p->alloced_count);
    uint32_t user_stack_top = 0x00800000;
    p->user_esp = user_stack_top;

    /* Build dummy interrupt frame on kernel stack (SS3, ESP3, EFLAGS, CS3, EIP) */
    uint32_t *stack = (uint32_t *)p->kernel_stack_top;
    *(--stack) = SEG_UDATA;             /* SS3  (User Data | RPL=3) */
    *(--stack) = user_stack_top;        /* ESP3 (top of user stack) */
    *(--stack) = 0x0202;                /* EFLAGS (IF=1)            */
    *(--stack) = SEG_UCODE;             /* CS   (User Code | RPL=3) */
    *(--stack) = entry_point;           /* EIP  (Ring 3 entry point)*/

    /* pushad register frame (all zeroes) */
    *(--stack) = 0;   /* EDI */
    *(--stack) = 0;   /* ESI */
    *(--stack) = 0;   /* EBP */
    *(--stack) = 0;   /* ESP (ignored by popad) */
    *(--stack) = 0;   /* EBX */
    *(--stack) = 0;   /* EDX */
    *(--stack) = 0;   /* ECX */
    *(--stack) = 0;   /* EAX */

    p->esp   = (uint32_t)stack;
    p->state = PROC_STATE_READY;
    return p;
}
