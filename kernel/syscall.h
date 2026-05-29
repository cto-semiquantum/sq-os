#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

#define SYS_PRINT  1
#define SYS_MALLOC 2
#define SYS_FREE   3
#define SYS_TIME   4
#define SYS_EXIT   5

/* syscall_init — initialize syscall statistics and subsystem. */
void syscall_init(void);

/* get_syscall_count — returns the number of times a syscall has been invoked. */
uint32_t get_syscall_count(uint32_t syscall_num);

/* syscall_handler — main dispatcher called from entry.asm. */
uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

/* Core kernel implementations of the system calls */
void sys_print(const char *str);
void *sys_malloc(size_t size);
void sys_free(void *ptr);
uint32_t sys_time(int *hours, int *minutes);
void sys_exit(int code);

#endif /* SYSCALL_H */
