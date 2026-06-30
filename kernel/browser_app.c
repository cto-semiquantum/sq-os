#include "window_manager.h"
#include "graphics.h"
#include "fat12.h"
#include "../net/tcp.h"
#include "../net/net.h"

// Define limits
#define URL_MAX_LEN 64
#define STATUS_MAX_LEN 64
#define RESPONSE_MAX_LEN 4096
#define MAX_WRAPPED_LINES 512

// Browser variables
static char current_url[URL_MAX_LEN] = "10.0.2.2";
static char status_msg[STATUS_MAX_LEN] = "Ready";
static char response_buf[RESPONSE_MAX_LEN] = "";
static int response_len = 0;
static int display_mode = 0; // 0 = TXT (stripped), 1 = SRC (raw)
static int scroll_offset = 0;

// Shift and regular scancode maps
static const char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char shift_scancode_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

// String helpers
static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void str_copy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == '\0' && *b == '\0');
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

static void btoa(uint8_t val, char *out, int *len) {
    if (val == 0) { out[0] = '0'; *len = 1; return; }
    char tmp[4]; int n = 0;
    while (val > 0) { tmp[n++] = '0' + (val % 10); val /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    *len = n;
}

// Sunken box and 3D buttons
static void draw_sunken_box(int x, int y, int w, int h) {
    draw_rect(x, y, w, h, 15); // White body
    draw_rect(x, y, w, 1, 8);   // Dark gray top
    draw_rect(x, y, 1, h, 8);   // Dark gray left
    draw_rect(x, y + h - 1, w, 1, 15); // White bottom
    draw_rect(x + w - 1, y, 1, h, 15); // White right
}

static void draw_button(int x, int y, int w, int h, const char *label) {
    draw_rect(x, y, w, h, 7); // raised light gray
    draw_rect(x, y, w, 1, 15); // white top highlight
    draw_rect(x, y, 1, h, 15); // white left highlight
    draw_rect(x, y + h - 1, w, 1, 8); // dark gray bottom shadow
    draw_rect(x + w - 1, y, 1, h, 8); // dark gray right shadow
    
    int len = str_len(label);
    int lx = x + (w - len * 8) / 2;
    int ly = y + (h - 8) / 2;
    draw_text(lx, ly, label, 0); // Black text
}

// DNS & URL parsing
static int parse_ip(const char *str, uint8_t *ip_out) {
    int ip_part = 0;
    int i = 0;
    uint8_t temp_ip[4] = {0, 0, 0, 0};
    while (str[i] && ip_part < 4) {
        if (str[i] == '.') {
            ip_part++;
        } else if (str[i] >= '0' && str[i] <= '9') {
            int val = temp_ip[ip_part] * 10 + (str[i] - '0');
            if (val > 255) return 0;
            temp_ip[ip_part] = val;
        } else {
            return 0; // invalid character
        }
        i++;
    }
    if (ip_part == 3) {
        ip_out[0] = temp_ip[0];
        ip_out[1] = temp_ip[1];
        ip_out[2] = temp_ip[2];
        ip_out[3] = temp_ip[3];
        return 1;
    }
    return 0;
}

int dns_resolve(const char *hostname, uint8_t *ip_out) {
    if (parse_ip(hostname, ip_out)) {
        return 1;
    }
    if (str_eq(hostname, "google.com") || str_eq(hostname, "www.google.com")) {
        ip_out[0] = 142; ip_out[1] = 250; ip_out[2] = 190; ip_out[3] = 46;
        return 1;
    }
    if (str_eq(hostname, "example.com") || str_eq(hostname, "www.example.com")) {
        ip_out[0] = 93; ip_out[1] = 184; ip_out[2] = 216; ip_out[3] = 34;
        return 1;
    }
    if (str_eq(hostname, "localhost")) {
        ip_out[0] = 127; ip_out[1] = 0; ip_out[2] = 0; ip_out[3] = 1;
        return 1;
    }
    if (str_eq(hostname, "gateway") || str_eq(hostname, "repo.sqos.dev") || str_eq(hostname, "www.repo.sqos.dev")) {
        ip_out[0] = 10; ip_out[1] = 0; ip_out[2] = 2; ip_out[3] = 2;
        return 1;
    }
    return 0;
}

void parse_url(const char *url, char *host, char *path) {
    int start = 0;
    if (str_starts_with(url, "http://")) {
        start = 7;
    } else if (str_starts_with(url, "http ")) {
        start = 5;
    }
    
    int hp = 0;
    int i = start;
    while (url[i] && url[i] != '/' && url[i] != ' ') {
        if (hp < 63) host[hp++] = url[i];
        i++;
    }
    host[hp] = '\0';
    
    int pp = 0;
    if (url[i] == '/') {
        while (url[i] && url[i] != ' ') {
            if (pp < 127) path[pp++] = url[i];
            i++;
        }
    } else {
        path[pp++] = '/';
    }
    path[pp] = '\0';
}

// HTML tag stripper
static void strip_html_tags(const char *src, char *dest, int max_len) {
    int in_tag = 0;
    int dp = 0;
    int sp = 0;
    while (src[sp] && dp < max_len - 1) {
        if (src[sp] == '<') {
            in_tag = 1;
        } else if (src[sp] == '>') {
            in_tag = 0;
        } else if (!in_tag) {
            if (src[sp] != '\r') {
                dest[dp++] = src[sp];
            }
        }
        sp++;
    }
    dest[dp] = '\0';
}

// Find HTTP response body offset
static const char *get_http_body(const char *buf, int len, int *is_http) {
    if (len >= 5 && buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P' && buf[4] == '/') {
        *is_http = 1;
        for (int i = 0; i < len - 3; i++) {
            if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
                return buf + i + 4;
            }
        }
        for (int i = 0; i < len - 1; i++) {
            if (buf[i] == '\n' && buf[i+1] == '\n') {
                return buf + i + 2;
            }
        }
    } else {
        *is_http = 0;
    }
    return buf;
}

// Load URL implementation
void browser_load_url(void) {
    if (current_url[0] == '\0') {
        str_copy(status_msg, "URL is empty", STATUS_MAX_LEN);
        return;
    }
    
    char host[64] = "";
    char path[128] = "";
    parse_url(current_url, host, path);
    
    // Local Bookmarks check
    if (str_eq(host, "bookmarks") || str_eq(host, "bookmarks.txt")) {
        int r = fs_read_file("BOOKMARKS.TXT", (uint8_t *)response_buf, sizeof(response_buf) - 1);
        if (r >= 0) {
            response_buf[r] = '\0';
            response_len = r;
            str_copy(status_msg, "Loaded BOOKMARKS.TXT", STATUS_MAX_LEN);
        } else {
            str_copy(response_buf, "No bookmarks saved yet.\nClick BOOKMARK to save current URL.", sizeof(response_buf));
            response_len = str_len(response_buf);
            str_copy(status_msg, "No bookmarks file", STATUS_MAX_LEN);
        }
        scroll_offset = 0;
        return;
    }
    
    // Resolve DNS
    uint8_t dest_ip[4] = {0,0,0,0};
    str_copy(status_msg, "Resolving host...", STATUS_MAX_LEN);
    
    // Refresh screen to show status
    extern void redraw_desktop(void);
    redraw_desktop();
    
    if (!dns_resolve(host, dest_ip)) {
        str_copy(status_msg, "DNS resolve failed", STATUS_MAX_LEN);
        return;
    }
    
    // Build connection status string
    char ip_str[40];
    str_copy(ip_str, "Connecting to ", sizeof(ip_str));
    int ip_len = str_len(ip_str);
    for (int j = 0; j < 4; j++) {
        int l; char tmp[4];
        btoa(dest_ip[j], tmp, &l);
        for (int k = 0; k < l; k++) {
            if (ip_len < (int)sizeof(ip_str) - 2) ip_str[ip_len++] = tmp[k];
        }
        if (j < 3 && ip_len < (int)sizeof(ip_str) - 2) ip_str[ip_len++] = '.';
    }
    ip_str[ip_len] = '\0';
    str_copy(status_msg, ip_str, STATUS_MAX_LEN);
    redraw_desktop();
    
    // Open TCP Connection (port 80)
    int sock = tcp_connect(dest_ip, 80);
    if (sock < 0) {
        str_copy(status_msg, "TCP connect failed", STATUS_MAX_LEN);
        return;
    }
    
    str_copy(status_msg, "CONNECTED", STATUS_MAX_LEN);
    redraw_desktop();
    
    // Format HTTP/1.1 GET request
    static char http_req[256];
    int rp = 0;
    const char *get_str = "GET ";
    for (int j = 0; get_str[j]; j++) http_req[rp++] = get_str[j];
    for (int j = 0; path[j] && rp < 120; j++) http_req[rp++] = path[j];
    const char *ver_str = " HTTP/1.1\r\nHost: ";
    for (int j = 0; ver_str[j]; j++) http_req[rp++] = ver_str[j];
    for (int j = 0; host[j] && rp < 200; j++) http_req[rp++] = host[j];
    const char *end_str = "\r\nConnection: close\r\n\r\n";
    for (int j = 0; end_str[j] && rp < 250; j++) http_req[rp++] = end_str[j];
    http_req[rp] = '\0';
    
    tcp_send(sock, (const uint8_t *)http_req, rp);
    
    str_copy(status_msg, "RECEIVING", STATUS_MAX_LEN);
    redraw_desktop();
    
    // Read response loop
    extern volatile uint32_t system_ticks;
    uint32_t start_ticks = system_ticks;
    response_len = 0;
    response_buf[0] = '\0';
    int got_data = 0;
    
    while (system_ticks - start_ticks < 72) {
        extern void net_poll(void);
        net_poll();
        
        static uint8_t rx_tmp[512];
        int rlen = tcp_recv(sock, rx_tmp, sizeof(rx_tmp));
        if (rlen > 0) {
            start_ticks = system_ticks;
            got_data = 1;
            if (response_len + rlen < RESPONSE_MAX_LEN - 1) {
                for (int j = 0; j < rlen; j++) {
                    response_buf[response_len++] = (char)rx_tmp[j];
                }
                response_buf[response_len] = '\0';
            } else {
                int fit = RESPONSE_MAX_LEN - 1 - response_len;
                for (int j = 0; j < fit; j++) {
                    response_buf[response_len++] = (char)rx_tmp[j];
                }
                response_buf[response_len] = '\0';
                break;
            }
        } else if (rlen < 0) {
            break;
        } else {
            for (volatile int d = 0; d < 1000; d++);
        }
    }
    
    tcp_close(sock);
    
    if (got_data) {
        str_copy(status_msg, "COMPLETE", STATUS_MAX_LEN);
    } else {
        str_copy(status_msg, "No response", STATUS_MAX_LEN);
    }
    scroll_offset = 0;
}

// Bookmarks and downloads functions
static void browser_bookmark_current(void) {
    if (current_url[0] == '\0') {
        str_copy(status_msg, "URL is empty", STATUS_MAX_LEN);
        return;
    }
    
    static char bookmarks_buf[1024];
    int r = fs_read_file("BOOKMARKS.TXT", (uint8_t *)bookmarks_buf, sizeof(bookmarks_buf) - 2);
    if (r < 0) {
        r = 0;
    }
    bookmarks_buf[r] = '\0';
    
    int url_len = str_len(current_url);
    if (r + url_len + 2 < (int)sizeof(bookmarks_buf)) {
        char *p = bookmarks_buf + r;
        if (r > 0 && bookmarks_buf[r - 1] != '\n') {
            *p++ = '\n';
        }
        str_copy(p, current_url, sizeof(bookmarks_buf) - (p - bookmarks_buf));
        p += url_len;
        *p++ = '\n';
        *p = '\0';
        
        int written = fs_write_file("BOOKMARKS.TXT", (const uint8_t *)bookmarks_buf, p - bookmarks_buf);
        if (written >= 0) {
            str_copy(status_msg, "Bookmarked!", STATUS_MAX_LEN);
        } else {
            str_copy(status_msg, "Bookmark write err", STATUS_MAX_LEN);
        }
    } else {
        str_copy(status_msg, "Bookmarks full", STATUS_MAX_LEN);
    }
}

static void browser_download_page(void) {
    if (response_len == 0) {
        str_copy(status_msg, "No page to save", STATUS_MAX_LEN);
        return;
    }
    
    int written = fs_write_file("PAGE.TXT", (const uint8_t *)response_buf, response_len);
    if (written >= 0) {
        str_copy(status_msg, "Saved to PAGE.TXT", STATUS_MAX_LEN);
    } else {
        str_copy(status_msg, "Save failed", STATUS_MAX_LEN);
    }
}

// Dynamic text line wrap scanner
static int get_lines(const char *text, int *line_starts, int max_lines) {
    int line_count = 0;
    int i = 0;
    int line_len = 0;
    line_starts[0] = 0;
    line_count = 1;
    
    while (text[i] && line_count < max_lines) {
        if (text[i] == '\n') {
            line_starts[line_count++] = i + 1;
            line_len = 0;
        } else {
            line_len++;
            if (line_len >= 28) { // wrap at 28 characters (8px font in 226px area)
                line_starts[line_count++] = i + 1;
                line_len = 0;
            }
        }
        i++;
    }
    return line_count;
}

// Content area drawing
void draw_browser_content(Window *win) {
    // 1. Light gray body
    draw_rect(win->x + 4, win->y + 14, win->w - 8, win->h - 18, 7);
    draw_rect(win->x + 4, win->y + 14, win->w - 8, 1, 8); // Separator under titlebar
    
    // 2. URL Bar and GO button (Y: win->y + 15 to win->y + 27)
    draw_sunken_box(win->x + 8, win->y + 15, 207, 12);
    
    // Clip URL text inside box
    draw_text_clipped(win->x + 12, win->y + 17, current_url, 0, 
                      win->x + 10, win->x + 213, win->y + 15, win->y + 27);
    
    // Animate active cursor in URL bar if window is active and URL is short
    static int cursor_tick = 0;
    cursor_tick++;
    if (win->active && (cursor_tick & 31) < 20) {
        int url_len = str_len(current_url);
        int cur_x = win->x + 12 + url_len * 8;
        if (cur_x < win->x + 210) {
            draw_text_clipped(cur_x, win->y + 17, "_", 0, 
                              win->x + 10, win->x + 213, win->y + 15, win->y + 27);
        }
    }
    
    draw_button(win->x + 220, win->y + 15, 32, 12, "GO");
    
    // 3. Navigation Bar Buttons (Y: win->y + 31 to win->y + 43)
    draw_button(win->x + 8, win->y + 31, 20, 12, "UP");  // Scroll Up
    draw_button(win->x + 32, win->y + 31, 20, 12, "DN"); // Scroll Down
    
    // Mode button
    const char *mode_lbl = (display_mode == 0) ? "TXT" : "SRC";
    draw_button(win->x + 56, win->y + 31, 50, 12, mode_lbl);
    
    draw_button(win->x + 110, win->y + 31, 75, 12, "BOOKMARK");
    draw_button(win->x + 190, win->y + 31, 50, 12, "SAVE");
    
    // 4. Content Area (Y: win->y + 47 to win->y + 128)
    int content_box_x = win->x + 8;
    int content_box_y = win->y + 47;
    int content_box_w = 244;
    int content_box_h = 81;
    draw_sunken_box(content_box_x, content_box_y, content_box_w, content_box_h);
    
    // Prepare render text depending on display_mode
    static char render_buf[RESPONSE_MAX_LEN];
    int is_http = 0;
    const char *body_ptr = get_http_body(response_buf, response_len, &is_http);
    
    if (display_mode == 0) {
        // TXT: HTML tags stripped
        strip_html_tags(body_ptr, render_buf, sizeof(render_buf));
    } else {
        // SRC: Raw HTML (including headers for transparency)
        str_copy(render_buf, response_buf, sizeof(render_buf));
    }
    
    // Scan wrapped lines
    static int line_starts[MAX_WRAPPED_LINES];
    int total_lines = get_lines(render_buf, line_starts, MAX_WRAPPED_LINES);
    
    // Adjust scroll clamp
    if (scroll_offset > total_lines - 8) {
        scroll_offset = total_lines - 8;
    }
    if (scroll_offset < 0) {
        scroll_offset = 0;
    }
    
    // Render lines (max 8 visible, font height 10)
    int view_min_x = content_box_x + 4;
    int view_max_x = content_box_x + 230; // Leave 14px scrollbar space
    int view_min_y = content_box_y + 3;
    int view_max_y = content_box_y + content_box_h - 3;
    
    for (int i = 0; i < 8 && (scroll_offset + i) < total_lines; i++) {
        int line_idx = scroll_offset + i;
        int start_pos = line_starts[line_idx];
        int end_pos = render_buf[start_pos] ? line_starts[line_idx + 1] : start_pos;
        if (end_pos <= start_pos) {
            end_pos = start_pos;
            while (render_buf[end_pos] && render_buf[end_pos] != '\n' && (end_pos - start_pos) < 28) {
                end_pos++;
            }
        }
        
        // Extract line segment
        static char line_segment[40];
        int lp = 0;
        for (int k = start_pos; k < end_pos && lp < 39; k++) {
            char c = render_buf[k];
            if (c != '\n' && c != '\r') {
                line_segment[lp++] = c;
            }
        }
        line_segment[lp] = '\0';
        
        draw_text_clipped(content_box_x + 4, content_box_y + 4 + i * 9, line_segment, 0,
                          view_min_x, view_max_x, view_min_y, view_max_y);
    }
    
    // 5. Render scrollbar track and thumb on right
    int sb_x = content_box_x + content_box_w - 10;
    draw_rect(sb_x, content_box_y + 1, 9, content_box_h - 2, 8); // Dark gray track
    
    if (total_lines > 8) {
        int thumb_h = (content_box_h * 8) / total_lines;
        if (thumb_h < 10) thumb_h = 10;
        int track_h = content_box_h - 2 - thumb_h;
        int thumb_y = content_box_y + 1 + (scroll_offset * track_h) / (total_lines - 8);
        
        // Draw 3D thumb
        draw_rect(sb_x, thumb_y, 9, thumb_h, 7);
        draw_rect(sb_x, thumb_y, 9, 1, 15); // Top highlight
        draw_rect(sb_x, thumb_y, 1, thumb_h, 15); // Left highlight
        draw_rect(sb_x, thumb_y + thumb_h - 1, 9, 1, 0); // Shadow
        draw_rect(sb_x + 8, thumb_y, 1, thumb_h, 0); // Shadow
    }
    
    // 6. Status Bar (Y: win->y + 133 to win->y + 141)
    draw_rect(win->x + 4, win->y + win->h - 9, win->w - 8, 8, 7); // Light gray status bar
    draw_text_clipped(win->x + 8, win->y + win->h - 9, status_msg, 0,
                      win->x + 6, win->x + win->w - 6, win->y + win->h - 10, win->y + win->h);
}

// Clicks router
void browser_handle_click(int rel_x, int rel_y) {
    // GO Button (X: 220 to 252, Y: 15 to 27)
    if (rel_x >= 220 && rel_x <= 252 && rel_y >= 15 && rel_y <= 27) {
        browser_load_url();
        return;
    }
    
    // Scroll Up Button (X: 8 to 28, Y: 31 to 43)
    if (rel_x >= 8 && rel_x <= 28 && rel_y >= 31 && rel_y <= 43) {
        if (scroll_offset > 0) scroll_offset--;
        return;
    }
    
    // Scroll Down Button (X: 32 to 52, Y: 31 to 43)
    if (rel_x >= 32 && rel_x <= 52 && rel_y >= 31 && rel_y <= 43) {
        scroll_offset++;
        return;
    }
    
    // MODE Button (X: 56 to 106, Y: 31 to 43)
    if (rel_x >= 56 && rel_x <= 106 && rel_y >= 31 && rel_y <= 43) {
        display_mode = !display_mode;
        scroll_offset = 0;
        return;
    }
    
    // BOOKMARK Button (X: 110 to 185, Y: 31 to 43)
    if (rel_x >= 110 && rel_x <= 185 && rel_y >= 31 && rel_y <= 43) {
        browser_bookmark_current();
        return;
    }
    
    // SAVE Button (X: 190 to 240, Y: 31 to 43)
    if (rel_x >= 190 && rel_x <= 240 && rel_y >= 31 && rel_y <= 43) {
        browser_download_page();
        return;
    }
}

// Keyboard input processing
void browser_handle_key(uint8_t scancode) {
    static int shift_active = 0;
    if (scancode == 0x2A || scancode == 0x36) {
        shift_active = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_active = 0;
        return;
    }
    if (scancode & 0x80) return; // key release
    if (scancode >= 128) return;
    
    char c = shift_active ? shift_scancode_map[scancode] : scancode_map[scancode];
    if (c == 0) return;
    
    if (c == '\b') {
        int len = str_len(current_url);
        if (len > 0) {
            current_url[len - 1] = '\0';
        }
    } else if (c == '\n') {
        browser_load_url();
    } else {
        int len = str_len(current_url);
        if (len < URL_MAX_LEN - 2) {
            current_url[len] = c;
            current_url[len + 1] = '\0';
        }
    }
}
