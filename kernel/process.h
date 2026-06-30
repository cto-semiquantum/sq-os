#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/* Define jmp_buf layout: 6 registers (EBP, EBX, EDI, ESI, ESP, EIP) */
typedef uint32_t jmp_buf[6];

/* Assembly setjmp / longjmp helper functions */
int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

/* ── Process States ─────────────────────────────────────────── */
typedef enum {
    PROC_STATE_UNUSED = 0,
    PROC_STATE_CREATED,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_SLEEPING,
    PROC_STATE_TERMINATED
} ProcessState;

/* ── Scheduling Priorities ──────────────────────────────────── */
#define PRIO_HIGH   3    /* GUI / system task                    */
#define PRIO_NORMAL 2    /* Normal user applications             */
#define PRIO_LOW    1
#define PRIO_IDLE   0

/* ── Process Control Block ──────────────────────────────────── */
typedef struct {
    uint32_t  id;
    char      name[32];
    ProcessState state;
    uint32_t  entry_point;
    uint32_t  size;

    /* ── Kernel-mode stack (used during interrupt handling) ── */
    uint8_t  *stack_mem;         /* Allocated 4KB kernel stack buffer */
    uint32_t  esp;               /* Saved kernel-mode ESP             */
    uint32_t  kernel_stack_top;  /* Constant top of kernel stack (= stack_mem + 4096) */

    /* ── User-mode stack (used when running in Ring 3) ─────── */
    uint8_t  *user_stack_mem;    /* Allocated 4KB user stack buffer (Ring 3 only) */
    uint32_t  user_esp;          /* Saved user-mode ESP (informational)           */

    /* ── Privilege level ─────────────────────────────────────── */
    uint8_t   ring;              /* 0 = kernel task, 3 = user-mode task */

    /* ── Scheduler metadata ──────────────────────────────────── */
    uint32_t  priority;          /* PRIO_HIGH … PRIO_IDLE */
    uint32_t  sleep_ticks;       /* Ticks remaining in SLEEPING state */

    /* ── Memory Mapping & Dynamic Allocations (Paging/ELF) ─── */
    uint32_t  cr3;               /* Physical address of page directory (0 = global) */
    void     *alloced_blocks[32];/* Track raw allocated pointers to free on exit */
    int       alloced_count;     /* Count of raw allocations */
    uint32_t  user_heap_ptr;     /* Dynamic user heap (sbrk-style malloc) */
} Process;

#define MAX_PROCESSES 8
extern Process  process_table[MAX_PROCESSES];
extern Process *current_process;

void     process_init(void);
Process *process_create(const char *name, uint32_t entry_point,
                        uint32_t size, uint8_t ring,
                        uint32_t cr3, uint32_t user_stack_top,
                        uint32_t user_heap_start);
Process *process_create_elf(Process *p, const char *name, uint32_t entry_point,
                            uint32_t size);
void     process_exit(int code);
void     sys_sleep(uint32_t ticks);
void     sys_sleep_ms(uint32_t ms);
uint32_t scheduler_switch(uint32_t current_esp);
void     update_tss_esp0(void);

/* Page-fault crash handler — called from page_fault_isr in entry.asm */
int      page_fault_handler(uint32_t error_code, uint32_t fault_addr);

#endif /* PROCESS_H */
