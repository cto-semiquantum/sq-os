#ifndef WALLPAPER_H
#define WALLPAPER_H

/* render_wallpaper — blit the embedded 320x200 wallpaper pixel data into
 * the backbuffer. Call this at the start of redraw_desktop() after
 * clear_backbuffer(), so all subsequent draws (taskbar, windows, cursor)
 * appear on top of the background. */
void render_wallpaper(void);

#endif /* WALLPAPER_H */
