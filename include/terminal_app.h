#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include "window_manager.h"

/* ============================================================
 * Terminal configuration constants
 * ============================================================ */
#define TERM_HISTORY_LINES   8    /* visible output lines                */
#define TERM_HISTORY_LEN    40    /* max chars per history line          */
#define TERM_INPUT_MAX      30    /* max chars in active input buffer    */
#define TERM_CMD_HIST_SIZE   8    /* number of previous commands to save */

/* ============================================================
 * Terminal state — exported for diagnostic use
 * ============================================================ */
extern char     term_input[TERM_INPUT_MAX + 2];
extern uint32_t term_input_len;

/* Output history ring (TERM_HISTORY_LINES rows × TERM_HISTORY_LEN cols) */
extern char term_history[TERM_HISTORY_LINES][TERM_HISTORY_LEN];

/* ============================================================
 * Public API
 * ============================================================ */
void init_terminal_app(void);
void terminal_handle_key(uint8_t scancode);
void draw_terminal_content(Window *win);
void terminal_execute_command(const char *cmd);

#endif /* TERMINAL_APP_H */
