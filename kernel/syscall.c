#include "syscall.h"
#include "kernel.h"
#include "memory.h"
#include "rtc.h"
#include "process.h"
#include "terminal_app.h"

static uint32_t syscall_counts[6] = {0};

void syscall_init(void) {
    for (int i = 0; i < 6; i++) {
        syscall_counts[i] = 0;
    }
}

uint32_t get_syscall_count(uint32_t syscall_num) {
    if (syscall_num < 6) return syscall_counts[syscall_num];
    return 0;
}

uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    if (num > 0 && num < 6) {
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

void *sys_malloc(size_t size) {
    return kmalloc(size);
}

void sys_free(void *ptr) {
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
