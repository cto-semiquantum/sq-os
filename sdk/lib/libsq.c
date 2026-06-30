#include "sqos.h"

void sq_print(const char *str) {
    __asm__ volatile("mov $1, %%eax; mov %0, %%ebx; int $0x80" : : "r"(str) : "eax", "ebx");
}

void *sq_malloc(size_t size) {
    void *ptr;
    __asm__ volatile("mov $2, %%eax; mov %1, %%ebx; int $0x80; mov %%eax, %0" : "=r"(ptr) : "r"(size) : "eax", "ebx");
    return ptr;
}

void sq_free(void *ptr) {
    __asm__ volatile("mov $3, %%eax; mov %0, %%ebx; int $0x80" : : "r"(ptr) : "eax", "ebx");
}

uint32_t sq_time(int *hours, int *minutes) {
    uint32_t val;
    __asm__ volatile("mov $4, %%eax; mov %1, %%ebx; mov %2, %%ecx; int $0x80; mov %%eax, %0" 
                     : "=r"(val) : "r"(hours), "r"(minutes) : "eax", "ebx", "ecx");
    return val;
}

void sq_exit(int code) {
    __asm__ volatile("mov $5, %%eax; mov %0, %%ebx; int $0x80" : : "r"(code) : "eax", "ebx");
    while (1);
}

void sq_sleep(uint32_t ms) {
    uint32_t ticks = ms / 55;
    if (ticks == 0 && ms > 0) ticks = 1;
    __asm__ volatile("mov $6, %%eax; mov %0, %%ebx; int $0x80" : : "r"(ticks) : "eax", "ebx");
}
