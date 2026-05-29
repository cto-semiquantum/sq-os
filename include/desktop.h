#ifndef DESKTOP_H
#define DESKTOP_H

#include "window_manager.h"
#include "mouse.h"   /* resolved via -Idrivers */

// Content Drawers
void draw_welcome_content(Window *win);
void draw_files_content(Window *win);
void draw_settings_content(Window *win);
void draw_taskmanager_content(Window *win);
void draw_notes_content(Window *win);
void notes_handle_key(uint8_t scancode);
void draw_calc_content(Window *win);
void calc_press(char c);

// Main Desktop Drawing and Interaction
void redraw_desktop(void);
void handle_click_event(int cx, int cy);
void run_gui_loop(void);

#endif // DESKTOP_H
