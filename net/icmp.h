#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

struct icmp_header {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed));

void icmp_send_echo_request(const uint8_t *dest_ip, uint16_t id, uint16_t seq);
void icmp_handle_packet(const uint8_t *src_ip, const uint8_t *packet, uint16_t len);

extern volatile int icmp_reply_received;
extern volatile uint32_t icmp_reply_ms;

#endif
