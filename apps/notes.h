#ifndef NOTES_H
#define NOTES_H

#include <stdint.h>
#include "window_manager.h"

#define NOTES_MAX_SIZE 4096

extern char notes_text[NOTES_MAX_SIZE];
extern uint32_t notes_text_len;

void notes_open(void);
void notes_save(void);
void notes_clear(void);
void notes_handle_key(uint8_t scancode);
void draw_notes_content(Window *win);

#endif // NOTES_H
