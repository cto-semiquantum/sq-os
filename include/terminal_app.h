#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include "window_manager.h"

// Terminal state variables
extern char term_input[64];
extern uint32_t term_input_len;
extern char term_history[5][32];

// Functions
void init_terminal_app(void);
void terminal_handle_key(uint8_t scancode);
void draw_terminal_content(Window *win);
void terminal_execute_command(const char *cmd);

#endif // TERMINAL_APP_H
