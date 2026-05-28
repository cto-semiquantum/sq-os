#ifndef DESKTOP_H
#define DESKTOP_H

#include "window_manager.h"
#include "mouse.h"

// Content Drawers
void draw_welcome_content(Window *win);
void draw_files_content(Window *win);
void draw_settings_content(Window *win);

// Main Desktop Drawing and Interaction
void redraw_desktop(void);
void handle_click_event(int cx, int cy);
void run_gui_loop(void);

#endif // DESKTOP_H
