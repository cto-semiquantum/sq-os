#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>

/* ================================================================
 * SQ-OS Program Loader  (loader.h)
 * ================================================================
 *
 * Architecture
 * ------------
 * Programs are flat 32-bit position-independent binaries (no ELF/
 * PE headers). Each binary uses the call/pop trick to reference
 * embedded data regardless of load address.
 *
 * Disk layout (raw sectors in os.img):
 *
 *   Sector  0         Boot sector (boot/boot.asm)
 *   Sectors 1–49      Kernel binary (kernel.bin)
 *   Sector  50        App directory  (8 × 64-byte entries)
 *   Sector  51+       App binaries   (one per entry)
 *
 * App directory entry layout (64 bytes, little-endian):
 *
 *   Offset  0  char[12]   Filename  e.g. "HELLO.APP\0\0\0"
 *   Offset 12  uint32_t   First sector of binary on disk
 *   Offset 16  uint32_t   Binary size in bytes
 *   Offset 20  uint8_t[44] Reserved / padding
 *
 * Binary sector layout:
 *
 *   Bytes 0-3   uint32_t   Binary size (little-endian) ← redundant
 *                          safety copy; loader uses dir entry size
 *   Bytes 4+    Machine code (flat 32-bit PIC binary)
 *
 * Application ABI (32-bit cdecl)
 * --------------------------------
 *   void entry(char *out_buf, uint32_t buf_size);
 *
 *   out_buf   : write null-terminated output string here
 *   buf_size  : maximum bytes available in out_buf
 *
 * Execution model
 * ---------------
 *   1. load_program() reads the app directory from sector 50.
 *   2. Matches the requested filename (case-insensitive).
 *   3. Allocates heap memory via kmalloc(binary_size).
 *   4. Reads binary sectors into the heap buffer.
 *   5. Casts to AppEntry and calls it (ring 0 flat PM — executable).
 *   6. Copies output to caller's buffer.
 *   7. Releases heap memory via kfree().
 *
 * ================================================================ */

/* Fixed sector for the app directory */
#define APP_DIR_SECTOR     200U
#define APP_DIR_MAX_ENTRIES 8

/* Maximum size of an app binary (safety cap) */
#define APP_MAX_BINARY_SIZE 32768U

/* Output buffer size (must match what terminal passes) */
#define APP_OUT_BUF_SIZE   64U

/* App entry-point function type */
typedef void (*AppEntry)(char *out_buf, uint32_t buf_size);

/* ----------------------------------------------------------------
 * load_program — load and execute a named application
 *
 * name     : filename to look up in the app directory (e.g. "hello.app")
 * out_buf  : caller-provided buffer for program output
 * buf_size : size of out_buf in bytes
 *
 * Returns:
 *   0   Success — out_buf contains null-terminated program output
 *  -1   App not found in directory
 *  -2   Disk I/O error
 *  -3   Out of heap memory
 * ---------------------------------------------------------------- */
int load_program(const char *name, char *out_buf, uint32_t buf_size);

/* ----------------------------------------------------------------
 * loader_list_apps — list app names from the directory
 * Fills out_names[][13] with up to max_count filenames.
 * Returns number of apps found.
 * ---------------------------------------------------------------- */
int loader_list_apps(char out_names[][13], int max_count);

#endif /* LOADER_H */
