#include "tcp.h"
#include "ip.h"
#include "net.h"
#include "arp.h"
#include "../include/kernel.h"

/* ============================================================
 * Single-connection TCP state machine for SQ-OS.
 *
 * We maintain exactly one TCP connection slot. If a second
 * tcp_connect() is called the old connection is discarded.
 * ============================================================ */

/* ---- helpers (no libc) ---- */
static void mem_copy(void *d, const void *s, uint32_t n) {
    uint8_t *dd = (uint8_t *)d;
    const uint8_t *ss = (const uint8_t *)s;
    for (uint32_t i = 0; i < n; i++) dd[i] = ss[i];
}

static int ip_eq(const uint8_t *a, const uint8_t *b) {
    return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3];
}

/* ---- TCP pseudo-header checksum ---- */
static uint16_t tcp_checksum(const uint8_t *src_ip, const uint8_t *dst_ip,
                              const uint8_t *tcp_seg, uint16_t tcp_len) {
    uint32_t sum = 0;

    /* Pseudo-header: src IP, dst IP, zero, proto=6, TCP length */
    for (int i = 0; i < 4; i += 2) {
        sum += (uint16_t)(src_ip[i] << 8 | src_ip[i+1]);
        sum += (uint16_t)(dst_ip[i] << 8 | dst_ip[i+1]);
    }
    sum += 6;           /* TCP protocol number */
    sum += tcp_len;

    /* TCP segment */
    const uint16_t *p = (const uint16_t *)tcp_seg;
    uint16_t rem = tcp_len;
    while (rem > 1) { sum += *p++; rem -= 2; }
    if (rem) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* ---- Connection state ---- */
typedef struct {
    uint8_t  state;
    uint8_t  remote_ip[4];
    uint16_t remote_port;
    uint16_t local_port;
    uint32_t seq;          /* our next sequence number */
    uint32_t ack;          /* next expected from remote */
    /* Receive ring buffer */
    uint8_t  rx_buf[TCP_RX_BUF_SIZE];
    uint16_t rx_head;
    uint16_t rx_tail;
} TcpConn;

static TcpConn g_conn;
static uint32_t g_tcp_local_port_counter = 49152; /* ephemeral port base */

/* ---- Internal send helper ---- */
static void tcp_send_segment(uint8_t flags, const uint8_t *data, uint16_t data_len) {
    /* Build TCP segment in a local buffer */
    static uint8_t seg_buf[1500];

    struct tcp_header *hdr = (struct tcp_header *)seg_buf;
    hdr->src_port    = htons(g_conn.local_port);
    hdr->dest_port   = htons(g_conn.remote_port);
    hdr->seq_num     = htonl(g_conn.seq);
    hdr->ack_num     = (flags & TCP_FLAG_ACK) ? htonl(g_conn.ack) : 0;
    hdr->data_offset = (5 << 4);  /* 5 dwords = 20 bytes, no options */
    hdr->flags       = flags;
    hdr->window_size = htons(TCP_RX_BUF_SIZE);
    hdr->checksum    = 0;
    hdr->urgent_ptr  = 0;

    if (data && data_len > 0) {
        mem_copy(seg_buf + 20, data, data_len);
    }

    uint16_t total_len = 20 + data_len;

    /* Fill in checksum using our IP + remote IP */
    extern uint8_t my_ip[4];
    hdr->checksum = tcp_checksum(my_ip, g_conn.remote_ip,
                                 seg_buf, total_len);

    ip_send(g_conn.remote_ip, IP_PROTO_TCP, seg_buf, total_len);
}

/* ============================================================
 * Public API
 * ============================================================ */

void tcp_init(void) {
    g_conn.state       = TCP_STATE_CLOSED;
    g_conn.rx_head     = 0;
    g_conn.rx_tail     = 0;
}

int tcp_connect(const uint8_t *dest_ip, uint16_t port) {
    /* Reset any existing connection */
    g_conn.state       = TCP_STATE_CLOSED;
    g_conn.rx_head     = 0;
    g_conn.rx_tail     = 0;

    g_conn.remote_port = port;
    g_conn.local_port  = (uint16_t)(g_tcp_local_port_counter++ & 0xFFFF);
    if (g_conn.local_port < 49152) g_conn.local_port = 49152;
    mem_copy(g_conn.remote_ip, dest_ip, 4);

    /* Initial sequence number (simple fixed value) */
    extern volatile uint32_t system_ticks;
    g_conn.seq = system_ticks * 0x1234 + 0xA5A5A5A5;
    g_conn.ack = 0;

    /* Send SYN */
    g_conn.state = TCP_STATE_SYN_SENT;
    tcp_send_segment(TCP_FLAG_SYN, 0, 0);

    /* Wait for SYN-ACK (up to ~3s = 54 ticks at 18Hz) */
    uint32_t start = system_ticks;
    uint32_t last_syn = start;
    while (system_ticks - start < 54) {
        extern void net_poll(void);
        net_poll();
        if (g_conn.state == TCP_STATE_ESTABLISHED) {
            return 0; /* socket 0 */
        }
        if (system_ticks - last_syn >= 18) {
            tcp_send_segment(TCP_FLAG_SYN, 0, 0);
            last_syn = system_ticks;
        }
    }

    g_conn.state = TCP_STATE_CLOSED;
    return -1; /* timeout */
}

int tcp_send(int sock, const uint8_t *data, uint16_t len) {
    (void)sock;
    if (g_conn.state != TCP_STATE_ESTABLISHED) return -1;
    if (!data || len == 0) return 0;

    tcp_send_segment(TCP_FLAG_ACK | TCP_FLAG_PSH, data, len);
    g_conn.seq += len;
    return (int)len;
}

int tcp_recv(int sock, uint8_t *buf, uint16_t max_len) {
    (void)sock;
    if (g_conn.state != TCP_STATE_ESTABLISHED) return -1;

    uint16_t avail = 0;
    if (g_conn.rx_tail >= g_conn.rx_head) {
        avail = g_conn.rx_tail - g_conn.rx_head;
    } else {
        avail = TCP_RX_BUF_SIZE - g_conn.rx_head + g_conn.rx_tail;
    }

    if (avail == 0) return 0;
    if (avail > max_len) avail = max_len;

    for (uint16_t i = 0; i < avail; i++) {
        buf[i] = g_conn.rx_buf[g_conn.rx_head];
        g_conn.rx_head = (g_conn.rx_head + 1) % TCP_RX_BUF_SIZE;
    }
    return (int)avail;
}

void tcp_close(int sock) {
    (void)sock;
    if (g_conn.state == TCP_STATE_ESTABLISHED) {
        tcp_send_segment(TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0);
        g_conn.seq++;
    }
    g_conn.state = TCP_STATE_CLOSED;
}

/* ============================================================
 * Incoming packet handler (called by ip.c)
 * ============================================================ */
void tcp_handle_packet(const uint8_t *src_ip, const uint8_t *packet, uint16_t len) {
    if (len < 20) return;

    const struct tcp_header *hdr = (const struct tcp_header *)packet;
    uint16_t src_port  = ntohs(hdr->src_port);
    uint16_t dst_port  = ntohs(hdr->dest_port);
    uint8_t  flags     = hdr->flags;
    uint32_t remote_seq = ntohl(hdr->seq_num);
    uint32_t remote_ack = ntohl(hdr->ack_num);

    /* Verify this packet is for our connection */
    if (!ip_eq(src_ip, g_conn.remote_ip)) return;
    if (src_port != g_conn.remote_port)   return;
    if (dst_port != g_conn.local_port)    return;

    uint8_t hdr_len = (hdr->data_offset >> 4) * 4;
    if (hdr_len > len) return;
    const uint8_t *data = packet + hdr_len;
    uint16_t data_len   = len - hdr_len;

    if (g_conn.state == TCP_STATE_SYN_SENT) {
        /* Expecting SYN-ACK */
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            g_conn.seq = remote_ack;               /* their ACK is our next seq */
            g_conn.ack = remote_seq + 1;           /* ack their SYN */
            g_conn.state = TCP_STATE_ESTABLISHED;
            /* Send ACK to complete handshake */
            tcp_send_segment(TCP_FLAG_ACK, 0, 0);
        }
        return;
    }

    if (g_conn.state == TCP_STATE_ESTABLISHED) {
        /* RST → close */
        if (flags & TCP_FLAG_RST) {
            g_conn.state = TCP_STATE_CLOSED;
            return;
        }

        /* Data from remote */
        if (data_len > 0) {
            g_conn.ack = remote_seq + data_len;
            /* Copy into receive ring buffer */
            for (uint16_t i = 0; i < data_len; i++) {
                uint16_t next = (g_conn.rx_tail + 1) % TCP_RX_BUF_SIZE;
                if (next != g_conn.rx_head) { /* don't overflow */
                    g_conn.rx_buf[g_conn.rx_tail] = data[i];
                    g_conn.rx_tail = next;
                }
            }
            /* Send ACK */
            tcp_send_segment(TCP_FLAG_ACK, 0, 0);
        }

        /* FIN from remote */
        if (flags & TCP_FLAG_FIN) {
            g_conn.ack = remote_seq + 1;
            tcp_send_segment(TCP_FLAG_ACK | TCP_FLAG_FIN, 0, 0);
            g_conn.seq++;
            g_conn.state = TCP_STATE_CLOSED;
        }

        (void)remote_ack; /* suppress unused warning */
    }
}
