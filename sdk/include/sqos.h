#ifndef SQOS_H
#define SQOS_H

#include <stdint.h>
#include <stddef.h>

/* SQ-OS User Mode C API */

/* sq_print — Print a string to the desktop console */
void sq_print(const char *str);

/* sq_malloc — Allocate memory from the process's user-space heap */
void *sq_malloc(size_t size);

/* sq_free — Free allocated memory (no-op in v1, auto-reclaimed on exit) */
void sq_free(void *ptr);

/* sq_time — Get system time (hours and minutes) */
uint32_t sq_time(int *hours, int *minutes);

/* sq_exit — Terminate the current process */
void sq_exit(int code);

/* sq_sleep — Sleep for the specified number of milliseconds */
void sq_sleep(uint32_t ms);

#endif /* SQOS_H */
