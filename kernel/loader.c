#include "loader.h"
#include "../fs/fat12.h"   /* disk_read_sector()  */
#include "memory.h"        /* kmalloc() / kfree() */
#include "process.h"       /* process context */
#include "window_manager.h"
#include "terminal_app.h"
#include "elf.h"

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
    DirEntry fe;
    uint8_t *code = (void *)0;
    uint32_t bin_size = 0;

    if (fat12_init() == 0 && fat12_find_file(name, &fe) == 0) {
        bin_size = fe.file_size;
        if (bin_size == 0 || bin_size > APP_MAX_BINARY_SIZE) return -2;

        code = (uint8_t *)kmalloc((size_t)bin_size);
        if (!code) return -3;

        int bytes_read = fs_read_file(name, code, bin_size);
        if (bytes_read < 0) {
            kfree(code);
            return -2;
        }
    } else {
        /* Fallback to raw app directory and sector reading */
        AppDirEntry dir[APP_DIR_MAX_ENTRIES];
        int dir_count = load_app_dir(dir, APP_DIR_MAX_ENTRIES);
        if (dir_count < 0) return -2; /* Disk error */

        int found_idx = -1;
        for (int i = 0; i < dir_count; i++) {
            if (ldr_str_eq_ci(dir[i].name, name)) {
                found_idx = i;
                break;
            }
        }
        if (found_idx < 0) return -1; /* Not found */

        AppDirEntry *app = &dir[found_idx];
        bin_size   = app->size;
        uint32_t bin_sector = app->sector;

        if (bin_size == 0 || bin_size > APP_MAX_BINARY_SIZE) return -2;

        code = (uint8_t *)kmalloc((size_t)bin_size);
        if (!code) return -3;

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
    }

    /* 6. Create process and register in ready queue */
    out_buf[0] = '\0';

    Process *p = (void *)0;

    /* Detect if loaded binary is an ELF executable */
    if (bin_size >= 4 && code[0] == 0x7F && code[1] == 'E' && code[2] == 'L' && code[3] == 'F') {
        /* Find a free slot in process table and reserve it */
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_table[i].state == PROC_STATE_UNUSED) {
                p = &process_table[i];
                p->state = PROC_STATE_CREATED;
                break;
            }
        }

        if (p == (void *)0) {
            kfree(code);
            append_history("DBG: Process table full!");
            return -3;
        }

        uint32_t *pd = (void *)0;
        uint32_t entry_point = elf_load(code, bin_size, p, &pd);
        if (entry_point == 0) {
            p->state = PROC_STATE_UNUSED;
            kfree(code);
            append_history("DBG: ELF parse failed!");
            return -2;
        }

        /* Create the Ring 3 ELF process structure */
        Process *p_ok = process_create_elf(p, name, entry_point, bin_size);
        if (p_ok == (void *)0) {
            p->state = PROC_STATE_UNUSED;
            kfree(code);
            append_history("DBG: ELF process creation failed!");
            return -3;
        }

        /* Since segments are copied, the raw file buffer can be freed immediately */
        kfree(code);
    } else {
        /* Determine privilege ring for legacy flat binaries: CRASH.APP runs in Ring 3 (user mode) */
        uint8_t proc_ring = 0;
        if (name[0] == 'C' && name[1] == 'R' && name[2] == 'A' &&
            name[3] == 'S' && name[4] == 'H') {
            proc_ring = 3;
        }

        p = process_create(name, (uint32_t)code, bin_size, proc_ring, 0, 0, 0);
        if (p == (void *)0) {
            kfree(code);
            append_history("DBG: Legacy process creation failed!");
            return -3;
        }
    }

    // Link to window if there's a title match (case-insensitive substring)
    for (int w = 0; w < NUM_WINDOWS; w++) {
        if (window_order[w]) {
            const char *title = window_order[w]->title;
            int match = 0;
            for (int i = 0; title[i] != '\0'; i++) {
                int j = 0;
                while (name[j] != '\0' && title[i + j] != '\0') {
                    char c1 = name[j];
                    char c2 = title[i + j];
                    if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
                    if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
                    if (c1 != c2) break;
                    j++;
                }
                if (name[j] == '\0' || (name[j] == '.' && name[j+1] == 'a' && name[j+2] == 'p' && name[j+3] == 'p')) {
                    match = 1;
                    break;
                }
            }
            if (match) {
                window_order[w]->pid = p->id;
                window_order[w]->visible = 1;
                focus_window(window_order[w]);
                break;
            }
        }
    }
    
    char launch_msg[64];
    int p_idx = 0;
    const char *lbl = "Launched background: ";
    while (lbl[p_idx]) { launch_msg[p_idx] = lbl[p_idx]; p_idx++; }
    int n_idx = 0;
    while (name[n_idx] && p_idx < 63) { launch_msg[p_idx++] = name[n_idx++]; }
    launch_msg[p_idx] = '\0';
    append_history(launch_msg);
    
    return 0;
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
