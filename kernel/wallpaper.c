#include "graphics.h"
#include "wallpaper.h"

/*
 * render_wallpaper — Procedural retro wallpaper renderer
 * -------------------------------------------------------
 * Generates the same retro teal/navy gradient + starfield that
 * gen_wallpaper.py produces, but computed on-the-fly each call.
 *
 * This avoids embedding a 64 KB .rodata array in the kernel binary.
 * The bootloader only loads 50 sectors (25 600 bytes), so any kernel
 * binary > 25 KB would result in a black screen / corrupted boot.
 *
 * Runtime cost: ~64 000 iterations of cheap arithmetic — completes in
 * a few milliseconds at 32-bit PM speeds, imperceptible to the user.
 *
 * BMP-from-disk path (future milestone):
 *   When FAT12 disk I/O is wired, replace this function with:
 *     1. fat12_open("WALL.BMP")
 *     2. Parse 54-byte BITMAPFILEHEADER + BITMAPINFOHEADER
 *     3. Program VGA DAC palette via ports 0x3C8/0x3C9
 *     4. Read rows bottom-up into draw_buffer
 */
uint8_t settings_wallpaper_style = 0;

void render_wallpaper(void) {
    uint8_t *buf = draw_buffer;

    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            uint8_t color = 1; // Default navy

            if (settings_wallpaper_style == 0) {
                /* Diagonal gradient: cycle navy(1) -> teal(3) -> blue(9) */
                int diag = (x * 2 + y * 3) & 0xFF;  /* 0-255 */
                if (diag < 80)        color = 1;   /* navy       */
                else if (diag < 160)  color = 3;   /* dark teal  */
                else                  color = 9;   /* bright blue*/

                /* CRT scanline dither: slightly darken odd rows in navy band */
                if ((y & 1) && diag < 80) color = 1;

                /* Starfield: cheap LCG hash, ~1 star per 350 pixels */
                unsigned int h = (unsigned int)(x * 0xAB4F + y * 0x4321 + x * y);
                if ((h % 350) == 0 && y > 10 && y < 190) {
                    color = 15;  /* white */
                }
            } else if (settings_wallpaper_style == 1) {
                /* Solid Teal */
                color = 3;
            } else if (settings_wallpaper_style == 2) {
                /* Starfield on Navy */
                color = 1;
                unsigned int h = (unsigned int)(x * 0xAB4F + y * 0x4321 + x * y);
                if ((h % 350) == 0 && y > 10 && y < 190) {
                    color = 15;  /* white */
                }
            }

            /* Horizon glow: bright cyan accent at desktop top edge */
            if (y == 10 || y == 11) color = 11;

            buf[y * VGA_WIDTH + x] = color;
        }
    }
}
