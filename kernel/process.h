#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

/* Define jmp_buf layout: 6 registers (EBP, EBX, EDI, ESI, ESP, EIP) */
typedef uint32_t jmp_buf[6];

/* Assembly setjmp / longjmp helper functions */
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

typedef enum {
    PROC_STATE_UNUSED = 0,
    PROC_STATE_CREATED,
    PROC_STATE_RUNNING,
    PROC_STATE_TERMINATED
} ProcessState;

typedef struct {
    uint32_t id;
    char name[32];
    ProcessState state;
    uint32_t entry_point;
    uint32_t size;
    jmp_buf exit_env; /* saved loader context for exiting */
} Process;

#define MAX_PROCESSES 8
extern Process process_table[MAX_PROCESSES];
extern Process *current_process;

void process_init(void);
Process *process_create(const char *name, uint32_t entry_point, uint32_t size);
void process_exit(int code);

#endif /* PROCESS_H */
