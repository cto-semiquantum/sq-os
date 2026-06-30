#ifndef TCP_H
#define TCP_H

#include <stdint.h>

/* ============================================================
 * SQ-OS Minimal TCP Stack
 *
 * Supports a single simultaneous connection (socket 0).
 * Enough for an HTTP/1.0 request-response cycle.
 * ============================================================ */

/* TCP header (20 bytes, no options) */
struct tcp_header {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset; /* upper 4 bits = header length in dwords */
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed));

/* TCP Flag bits */
#define TCP_FLAG_FIN  0x01
#define TCP_FLAG_SYN  0x02
#define TCP_FLAG_RST  0x04
#define TCP_FLAG_PSH  0x08
#define TCP_FLAG_ACK  0x10
#define TCP_FLAG_URG  0x20

/* Connection states */
#define TCP_STATE_CLOSED      0
#define TCP_STATE_SYN_SENT    1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_WAIT    3

/* Receive ring buffer size */
#define TCP_RX_BUF_SIZE 1024

/* Public API */

/* tcp_init — called from net_init() to reset connection state */
void tcp_init(void);

/* tcp_connect — initiate a connection to dest_ip:port.
 * Blocks up to ~3s waiting for SYN-ACK.
 * Returns 0 on success (the only valid socket id), -1 on failure. */
int tcp_connect(const uint8_t *dest_ip, uint16_t port);

/* tcp_send — send data on an established connection.
 * Returns number of bytes sent, or -1 on error. */
int tcp_send(int sock, const uint8_t *data, uint16_t len);

/* tcp_recv — copy received data into buf (non-blocking).
 * Returns number of bytes copied (0 if nothing available), -1 on error. */
int tcp_recv(int sock, uint8_t *buf, uint16_t max_len);

/* tcp_close — send FIN and mark connection closed. */
void tcp_close(int sock);

/* tcp_handle_packet — called by ip.c when IP_PROTO_TCP is received */
void tcp_handle_packet(const uint8_t *src_ip, const uint8_t *packet, uint16_t len);

#endif /* TCP_H */
