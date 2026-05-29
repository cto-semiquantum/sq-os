#include "loader.h"
#include "../fs/fat12.h"   /* disk_read_sector()  */
#include "memory.h"        /* kmalloc() / kfree() */
#include "process.h"       /* process context */
#include "terminal_app.h"

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

    /* 6. Execute: Copy to User Space (0x00400000), register process, and execute */
    out_buf[0] = '\0';
    
    append_history("DBG: Copying app...");
    uint8_t *user_space_code = (uint8_t *)0x00400000;
    for (uint32_t b = 0; b < bin_size; b++) {
        user_space_code[b] = code[b];
    }
    
    // Free the temporary heap allocation since we copied it to user space
    kfree(code);
    append_history("DBG: App copied to 0x400000");

    // Print stack pointer ESP
    uint32_t current_esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(current_esp));
    char esp_str[40];
    // Simple copy loop
    const char *esp_lbl = "DBG: ESP = 0x";
    int p_esp = 0;
    while (esp_lbl[p_esp]) { esp_str[p_esp] = esp_lbl[p_esp]; p_esp++; }
    char hex_esp[9];
    for (int j = 7; j >= 0; j--) {
        uint32_t val = (current_esp >> (j * 4)) & 0xF;
        if (val < 10) hex_esp[7 - j] = '0' + val;
        else hex_esp[7 - j] = 'A' + (val - 10);
    }
    hex_esp[8] = '\0';
    for (int j = 0; hex_esp[j] && p_esp < 39; j++) esp_str[p_esp++] = hex_esp[j];
    esp_str[p_esp] = '\0';
    append_history(esp_str);

    // Create process entry
    Process *p = process_create(name, (uint32_t)user_space_code, bin_size);
    if (p) {
        current_process = p;
        p->state = PROC_STATE_RUNNING;
        append_history("DBG: Process registered");
    }

    // Set exit longjmp handler. If sys_exit is called, it returns here.
    append_history("DBG: Saving context...");
    
    // Safety check: if process creation failed
    if (p == (void *)0) {
        append_history("DBG: Process create failed!");
        return -3;
    }

    int sj_ret = setjmp(p->exit_env);
    char sj_str[40];
    const char *sj_lbl = "DBG: setjmp ret = ";
    int p_sj = 0;
    while (sj_lbl[p_sj]) { sj_str[p_sj] = sj_lbl[p_sj]; p_sj++; }
    sj_str[p_sj++] = '0' + sj_ret;
    sj_str[p_sj] = '\0';
    append_history(sj_str);

    if (sj_ret == 0) {
        AppEntry entry = (AppEntry)user_space_code;
        append_history("DBG: Jumping to entry...");
        entry(out_buf, buf_size);
        append_history("DBG: Returned from entry");
    } else {
        append_history("DBG: Recovered from sys_exit");
    }

    if (p) {
        p->state = PROC_STATE_TERMINATED;
        current_process = (void *)0;
    }

    /* Null-terminate in case app forgot */
    out_buf[buf_size - 1] = '\0';

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
