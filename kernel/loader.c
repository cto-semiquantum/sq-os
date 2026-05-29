#include "loader.h"
#include "../fs/fat12.h"   /* disk_read_sector()  */
#include "memory.h"        /* kmalloc() / kfree() */

/* ================================================================
 * App directory entry structure (matches disk layout, 64 bytes)
 * ================================================================ */
typedef struct {
    char     name[12];      /* Filename, null-padded             */
    uint32_t sector;        /* First sector of binary on disk    */
    uint32_t size;          /* Binary size in bytes              */
    uint8_t  _pad[44];      /* Reserved                         */
} AppDirEntry;              /* Total: 64 bytes, 8 per sector     */

/* ================================================================
 * Internal helpers
 * ================================================================ */

/* Case-insensitive ASCII toupper */
static char ldr_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

/* Case-insensitive string compare */
static int ldr_str_eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (ldr_upper(*a) != ldr_upper(*b)) return 0;
        a++; b++;
    }
    return (ldr_upper(*a) == ldr_upper(*b));
}

/* Copy at most dst_size-1 chars, null-terminate */
static void ldr_str_copy(char *dst, const char *src, int dst_size) {
    int i = 0;
    while (src[i] && i < dst_size - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* ================================================================
 * load_app_dir — read sector 50 and parse 8 AppDirEntry records
 * Returns number of valid entries found.
 * ================================================================ */
static int load_app_dir(AppDirEntry *entries, int max_entries) {
    uint8_t sector[512];
    if (disk_read_sector(APP_DIR_SECTOR, sector) != 0) return -1;

    int count = 0;
    for (int i = 0; i < 8 && i < max_entries; i++) {
        AppDirEntry *e = (AppDirEntry *)(sector + i * 64);
        /* Empty entry: name starts with 0 */
        if (e->name[0] == '\0') break;
        /* Copy into output array */
        uint8_t *dst = (uint8_t *)&entries[i];
        uint8_t *src = (uint8_t *)e;
        for (int j = 0; j < 64; j++) dst[j] = src[j];
        count++;
    }
    return count;
}

/* ================================================================
 * load_program — load and execute a named application
 * ================================================================ */
int load_program(const char *name, char *out_buf, uint32_t buf_size) {
    /* 1. Read app directory */
    AppDirEntry dir[APP_DIR_MAX_ENTRIES];
    int dir_count = load_app_dir(dir, APP_DIR_MAX_ENTRIES);
    if (dir_count < 0) return -2; /* Disk error */

    /* 2. Find the requested app */
    int found_idx = -1;
    for (int i = 0; i < dir_count; i++) {
        if (ldr_str_eq_ci(dir[i].name, name)) {
            found_idx = i;
            break;
        }
    }
    if (found_idx < 0) return -1; /* Not found */

    AppDirEntry *app = &dir[found_idx];
    uint32_t bin_size   = app->size;
    uint32_t bin_sector = app->sector;

    /* 3. Sanity check */
    if (bin_size == 0 || bin_size > APP_MAX_BINARY_SIZE) return -2;

    /* 4. Allocate heap memory for binary */
    uint8_t *code = (uint8_t *)kmalloc((size_t)bin_size);
    if (!code) return -3;

    /* 5. Read binary sectors into heap buffer
     *    Each binary sector is plain data (no size prefix in binary sectors,
     *    the size came from the app directory).                              */
    uint32_t bytes_loaded = 0;
    uint32_t cur_sector   = bin_sector;
    uint8_t  sector_buf[512];

    while (bytes_loaded < bin_size) {
        if (disk_read_sector(cur_sector, sector_buf) != 0) {
            kfree(code);
            return -2;
        }
        uint32_t remaining = bin_size - bytes_loaded;
        uint32_t chunk     = (remaining < 512) ? remaining : 512;
        for (uint32_t b = 0; b < chunk; b++) {
            code[bytes_loaded + b] = sector_buf[b];
        }
        bytes_loaded += chunk;
        cur_sector++;
    }

    /* 6. Execute: cast heap buffer to function pointer and call
     *    Safe in flat 32-bit PM — no paging, no NX bit, ring 0.
     *    The app is PIC so any load address works.                */
    out_buf[0] = '\0';
    AppEntry entry = (AppEntry)code;
    entry(out_buf, buf_size);

    /* Null-terminate in case app forgot */
    out_buf[buf_size - 1] = '\0';

    /* 7. Release heap memory */
    kfree(code);

    return 0; /* Success */
}

/* ================================================================
 * loader_list_apps — list app names from the directory
 * ================================================================ */
int loader_list_apps(char out_names[][13], int max_count) {
    AppDirEntry dir[APP_DIR_MAX_ENTRIES];
    int count = load_app_dir(dir, APP_DIR_MAX_ENTRIES);
    if (count <= 0) return 0;

    int n = (count < max_count) ? count : max_count;
    for (int i = 0; i < n; i++) {
        ldr_str_copy(out_names[i], dir[i].name, 13);
    }
    return n;
}
