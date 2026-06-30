#include "sqpkg.h"
#include "memory.h"
#include "terminal_app.h"
#include "../fs/fat12.h"
#include "../net/net.h"
#include "../net/tcp.h"
#include "../net/ip.h"

/* ============================================================
 * String & Utility Helpers
 * ============================================================ */
static int sq_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void sq_strcpy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int sq_streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0');
}

static int sq_streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == '\0' && *b == '\0');
}

static int sq_strcontains_ci(const char *haystack, const char *needle) {
    if (!*needle) return 1;
    for (int i = 0; haystack[i]; i++) {
        int match = 1;
        for (int j = 0; needle[j]; j++) {
            if (!haystack[i + j]) { match = 0; break; }
            char ch = haystack[i + j];
            char cn = needle[j];
            if (ch >= 'A' && ch <= 'Z') ch += 32;
            if (cn >= 'A' && cn <= 'Z') cn += 32;
            if (ch != cn) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

static void sq_btoa(uint8_t val, char *out, int *len) {
    if (val == 0) { out[0]='0'; *len=1; return; }
    char tmp[4]; int n = 0;
    while (val > 0) { tmp[n++] = '0' + (val % 10); val /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    *len = n;
}

/* ============================================================
 * HTTP Downloader Helper
 * ============================================================ */
static int download_file(const char *host, const char *path, uint8_t *dest_buf, uint32_t max_size, uint32_t *out_size) {
    extern int dns_resolve(const char *hostname, uint8_t *ip_out);
    uint8_t dest_ip[4];
    if (!dns_resolve(host, dest_ip)) {
        append_history("DNS resolution failed");
        return -1;
    }

    int sock = tcp_connect(dest_ip, 80);
    if (sock < 0) {
        append_history("TCP connect failed");
        return -2;
    }

    /* Format GET request */
    static char req[512];
    int len = 0;
    const char *p1 = "GET ";
    while (*p1) req[len++] = *p1++;
    const char *p2 = path;
    while (*p2 && len < 200) req[len++] = *p2++;
    const char *p3 = " HTTP/1.1\r\nHost: ";
    while (*p3) req[len++] = *p3++;
    const char *p4 = host;
    while (*p4 && len < 350) req[len++] = *p4++;
    const char *p5 = "\r\nConnection: close\r\n\r\n";
    while (*p5) req[len++] = *p5++;
    req[len] = '\0';

    tcp_send(sock, (const uint8_t *)req, len);

    extern volatile uint32_t system_ticks;
    uint32_t start_ticks = system_ticks;
    int header_ended = 0;
    uint32_t body_bytes = 0;
    uint32_t header_state = 0;

    static uint8_t rx_buf[1024];

    while (system_ticks - start_ticks < 108) { /* 6-second timeout */
        extern void net_poll(void);
        net_poll();
        int r = tcp_recv(sock, rx_buf, sizeof(rx_buf));
        if (r > 0) {
            start_ticks = system_ticks; /* reset timeout on activity */
            for (int i = 0; i < r; i++) {
                if (!header_ended) {
                    uint8_t c = rx_buf[i];
                    if (header_state == 0 && c == '\r') header_state = 1;
                    else if (header_state == 1 && c == '\n') header_state = 2;
                    else if (header_state == 2 && c == '\r') header_state = 3;
                    else if (header_state == 3 && c == '\n') {
                        header_ended = 1;
                        header_state = 0;
                    } else {
                        header_state = 0;
                        if (c == '\r') header_state = 1;
                    }
                } else {
                    if (body_bytes < max_size) {
                        dest_buf[body_bytes++] = rx_buf[i];
                    }
                }
            }
        }
    }
    tcp_close(sock);
    *out_size = body_bytes;
    return (body_bytes > 0) ? 0 : -3;
}

/* ============================================================
 * Package Manager Commands
 * ============================================================ */
void sqpkg_execute(const char *subcmd, const char *arg) {
    if (sq_streq(subcmd, "update")) {
        append_history("Updating package database...");
        
        /* Allocate buffer for PACKAGES.TXT */
        uint8_t *db_buf = kmalloc(16384);
        if (!db_buf) {
            append_history("Out of memory");
            return;
        }

        uint32_t db_size = 0;
        int status = download_file("repo.sqos.dev", "/packages.txt", db_buf, 16383, &db_size);
        if (status != 0) {
            append_history("Failed to download database");
            kfree(db_buf);
            return;
        }

        db_buf[db_size] = '\0';

        /* Write database file to FAT12 */
        int w = fs_write_file("PACKAGES.TXT", db_buf, db_size);
        kfree(db_buf);

        if (w >= 0) {
            append_history("Repository database updated!");
        } else {
            append_history("Failed to save PACKAGES.TXT");
        }

    } else if (sq_streq(subcmd, "search")) {
        if (!arg || arg[0] == '\0') {
            append_history("Usage: sqpkg search <query>");
            return;
        }

        /* Read database */
        uint8_t *db_buf = kmalloc(16384);
        if (!db_buf) {
            append_history("Out of memory");
            return;
        }

        int r = fs_read_file("PACKAGES.TXT", db_buf, 16383);
        if (r < 0) {
            append_history("No database found. Run sqpkg update.");
            kfree(db_buf);
            return;
        }
        db_buf[r] = '\0';

        /* Parse database line by line: [name] [file] [desc] */
        char *line = (char *)db_buf;
        int found = 0;

        for (int i = 0; i < r; i++) {
            if (db_buf[i] == '\n' || db_buf[i] == '\0') {
                db_buf[i] = '\0';
                if (sq_strlen(line) > 0) {
                    /* Parse name */
                    char name[16]; int np = 0;
                    while (*line && *line != ' ' && np < 15) { name[np++] = *line++; }
                    name[np] = '\0';
                    if (*line == ' ') line++;

                    /* Parse file */
                    char file[32]; int fp = 0;
                    while (*line && *line != ' ' && fp < 31) { file[fp++] = *line++; }
                    file[fp] = '\0';
                    if (*line == ' ') line++;

                    /* Rest of line is description */
                    if (sq_strcontains_ci(name, arg) || sq_strcontains_ci(line, arg)) {
                        char print_line[80];
                        sq_strcpy(print_line, name, 80);
                        int p = sq_strlen(print_line);
                        while (p < 12) print_line[p++] = ' ';
                        sq_strcpy(print_line + p, line, 80 - p);
                        append_history(print_line);
                        found++;
                    }
                }
                line = (char *)&db_buf[i + 1];
            }
        }

        kfree(db_buf);
        if (found == 0) {
            append_history("No packages match query");
        }

    } else if (sq_streq(subcmd, "install")) {
        if (!arg || arg[0] == '\0') {
            append_history("Usage: sqpkg install <package>");
            return;
        }

        /* Read database to find package filename */
        uint8_t *db_buf = kmalloc(16384);
        if (!db_buf) {
            append_history("Out of memory");
            return;
        }

        int r = fs_read_file("PACKAGES.TXT", db_buf, 16383);
        if (r < 0) {
            append_history("No database. Run sqpkg update");
            kfree(db_buf);
            return;
        }
        db_buf[r] = '\0';

        char *line = (char *)db_buf;
        char target_file[32] = "";

        for (int i = 0; i < r; i++) {
            if (db_buf[i] == '\n' || db_buf[i] == '\0') {
                db_buf[i] = '\0';
                if (sq_strlen(line) > 0) {
                    char name[16]; int np = 0;
                    while (*line && *line != ' ' && np < 15) { name[np++] = *line++; }
                    name[np] = '\0';
                    if (*line == ' ') line++;

                    char file[32]; int fp = 0;
                    while (*line && *line != ' ' && fp < 31) { file[fp++] = *line++; }
                    file[fp] = '\0';

                    if (sq_streq_ci(name, arg)) {
                        sq_strcpy(target_file, file, 32);
                        break;
                    }
                }
                line = (char *)&db_buf[i + 1];
            }
        }
        kfree(db_buf);

        if (target_file[0] == '\0') {
            append_history("Package not found in repo");
            return;
        }

        /* Format remote path */
        char path[64] = "/packages/";
        sq_strcpy(path + 10, target_file, 54);

        char dl_msg[64];
        sq_strcpy(dl_msg, "Downloading ", 64);
        sq_strcpy(dl_msg + 12, target_file, 52);
        append_history(dl_msg);

        /* Allocate download buffer (32KB is plenty for small hobby apps) */
        uint8_t *pkg_buf = kmalloc(32768);
        if (!pkg_buf) {
            append_history("Out of memory");
            return;
        }

        uint32_t pkg_size = 0;
        int status = download_file("repo.sqos.dev", path, pkg_buf, 32767, &pkg_size);
        if (status != 0) {
            append_history("Download failed!");
            kfree(pkg_buf);
            return;
        }

        /* Unpack sequential SQF\0 format */
        append_history("Unpacking package...");
        uint32_t cursor = 0;
        int files_unpacked = 0;

        while (cursor + 40 <= pkg_size) {
            /* Verify magic SQF\0 */
            if (pkg_buf[cursor] != 'S' || pkg_buf[cursor+1] != 'Q' || pkg_buf[cursor+2] != 'F' || pkg_buf[cursor+3] != '\0') {
                break;
            }

            /* Read size */
            uint32_t fsize = *(uint32_t *)(pkg_buf + cursor + 4);

            /* Read filename (32 bytes) */
            char fname[32];
            for (int k = 0; k < 32; k++) {
                fname[k] = (char)pkg_buf[cursor + 8 + k];
            }
            fname[31] = '\0';

            cursor += 40;

            if (cursor + fsize > pkg_size) {
                append_history("Unpacking error: truncated file");
                break;
            }

            /* Write file to FAT12 */
            char log[64] = "  -> Writing ";
            sq_strcpy(log + 13, fname, 50);
            append_history(log);

            fs_write_file(fname, pkg_buf + cursor, fsize);
            cursor += fsize;
            files_unpacked++;
        }

        kfree(pkg_buf);

        if (files_unpacked > 0) {
            append_history("Installation complete!");
        } else {
            append_history("Failed to unpack archive.");
        }

    } else if (sq_streq(subcmd, "remove")) {
        if (!arg || arg[0] == '\0') {
            append_history("Usage: sqpkg remove <package>");
            return;
        }

        char elf_name[16];
        sq_strcpy(elf_name, arg, 12);
        int l = sq_strlen(elf_name);
        
        /* Append .ELF extension */
        sq_strcpy(elf_name + l, ".ELF", 5);

        append_history("Removing files...");
        int r1 = fs_delete_file(elf_name);

        if (r1 == 0) {
            append_history("Package removed successfully.");
        } else {
            append_history("Package not found / failed to remove.");
        }

    } else if (sq_streq(subcmd, "list")) {
        append_history("Installed ELF packages:");

        DirEntry entries[FAT12_MAX_FILES];
        int count = fat12_list_root(entries, FAT12_MAX_FILES);
        int found = 0;

        for (int i = 0; i < count; i++) {
            if (entries[i].name[0] == '\0' || entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr & FAT_ATTR_VOLUME_ID) continue;

            /* Check if extension is ELF */
            if (entries[i].ext[0] == 'E' && entries[i].ext[1] == 'L' && entries[i].ext[2] == 'F') {
                char line[40];
                int p = 0;
                for (int k = 0; k < 8 && entries[i].name[k] != ' '; k++) {
                    line[p++] = entries[i].name[k];
                }
                line[p++] = '.';
                line[p++] = 'E';
                line[p++] = 'L';
                line[p++] = 'F';
                line[p] = '\0';
                
                append_history(line);
                found++;
            }
        }

        if (found == 0) {
            append_history("  No package files installed.");
        }

    } else {
        append_history("Unknown sqpkg subcommand.");
        append_history("Subcommands: update search install remove list");
    }
}
