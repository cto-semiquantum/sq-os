#include "syscall.h"
#include "kernel.h"
#include "memory.h"
#include "rtc.h"
#include "process.h"
#include "terminal_app.h"

static uint32_t syscall_counts[7] = {0};

void syscall_init(void) {
    for (int i = 0; i < 7; i++) {
        syscall_counts[i] = 0;
    }
}

uint32_t get_syscall_count(uint32_t syscall_num) {
    if (syscall_num < 7) return syscall_counts[syscall_num];
    return 0;
}

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    if (num > 0 && num < 7) {
        syscall_counts[num]++;
    }

    switch (num) {
        case SYS_PRINT:
            sys_print((const char *)arg1);
            return 0;
        case SYS_MALLOC:
            return (uint32_t)sys_malloc((size_t)arg1);
        case SYS_FREE:
            sys_free((void *)arg1);
            return 0;
        case SYS_TIME:
            return sys_time((int *)arg1, (int *)arg2);
        case SYS_EXIT:
            sys_exit((int)arg1);
            return 0;
        case SYS_SLEEP:
            sys_sleep((uint32_t)arg1);
            return 0;
        default:
            return (uint32_t)-1;
    }
}

void sys_print(const char *str) {
    // Output to the GUI terminal history
    append_history(str);
    // Mirror to standard text console
    print_str(str, 0x07);
}

static void *user_space_malloc(Process *p, size_t size) {
    if (size == 0) return (void *)0;

    /* Align size to 8 bytes */
    size = (size + 7) & ~7;

    /* Verify we have slot space in tracking list to prevent overflow */
    if (p->alloced_count >= 30) {
        return (void *)0;
    }

    uint32_t ret_addr = p->user_heap_ptr;
    uint32_t next_heap = p->user_heap_ptr + size;

    /* Map pages if next_heap crosses the current page boundary */
    uint32_t current_mapped_page = (p->user_heap_ptr + 4095) & ~4095;
    uint32_t needed_mapped_page = (next_heap + 4095) & ~4095;

    extern void map_user_page(uint32_t *pd, uint32_t vaddr, uint32_t paddr, void **alloc_blocks, int *alloc_count);

    for (uint32_t vpage = current_mapped_page; vpage < needed_mapped_page; vpage += 4096) {
        /* Allocate physical page from kernel heap */
        uint8_t *raw = (uint8_t *)kmalloc(4096 * 2);
        if (!raw) return (void *)0;

        p->alloced_blocks[p->alloced_count++] = raw;
        uint32_t phys = ((uint32_t)raw + 4095) & ~4095;

        /* Zero initialize the physical page */
        for (int k = 0; k < 1024; k++) {
            ((uint32_t *)phys)[k] = 0;
        }

        /* Map it in the process's page directory */
        map_user_page((uint32_t *)p->cr3, vpage, phys, p->alloced_blocks, &p->alloced_count);
    }

    p->user_heap_ptr = next_heap;
    return (void *)ret_addr;
}

void *sys_malloc(size_t size) {
    if (current_process && current_process->ring == 3 && current_process->cr3 != 0) {
        return user_space_malloc(current_process, size);
    }
    return kmalloc(size);
}

void sys_free(void *ptr) {
    if (current_process && current_process->ring == 3 && current_process->cr3 != 0) {
        /* User-space heap is freed in bulk upon process exit.
         * For this simple sbrk implementation, free is a no-op. */
        return;
    }
    kfree(ptr);
}

uint32_t sys_time(int *hours, int *minutes) {
    int h = 0, m = 0;
    read_rtc(&h, &m);
    if (hours) *hours = h;
    if (minutes) *minutes = m;
    return (h << 8) | m;
}

void sys_exit(int code) {
    process_exit(code);
}
