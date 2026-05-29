#include "process.h"
#include "terminal_app.h"

Process process_table[MAX_PROCESSES];
Process *current_process = NULL;
static uint32_t next_pid = 1;

void process_init(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROC_STATE_UNUSED;
        process_table[i].id = 0;
        process_table[i].name[0] = '\0';
        process_table[i].entry_point = 0;
        process_table[i].size = 0;
    }
    current_process = (void *)0;
    next_pid = 1;
}

Process *process_create(const char *name, uint32_t entry_point, uint32_t size) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_STATE_UNUSED) {
            Process *p = &process_table[i];
            p->id = next_pid++;
            
            // Safe copy of process name
            int j = 0;
            while (name[j] && j < 31) {
                p->name[j] = name[j];
                j++;
            }
            p->name[j] = '\0';
            
            p->state = PROC_STATE_CREATED;
            p->entry_point = entry_point;
            p->size = size;
            return p;
        }
    }
    return (void *)0; // Process table full
}

void process_exit(int code) {
    if (current_process) {
        current_process->state = PROC_STATE_TERMINATED;

        // Build exit message: "FILENAME exited"
        char exit_msg[40];
        int len = 0;
        while (current_process->name[len] && len < 20) {
            exit_msg[len] = current_process->name[len];
            len++;
        }
        
        const char *suffix = " exited";
        int s = 0;
        while (suffix[s] && len < 39) {
            exit_msg[len++] = suffix[s++];
        }
        exit_msg[len] = '\0';
        
        // Output exit notification to terminal app
        append_history(exit_msg);

        // Cooperative context switch back to loader
        longjmp(current_process->exit_env, 1);
    }
}
