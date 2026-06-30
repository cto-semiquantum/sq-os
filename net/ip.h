#ifndef IP_H
#define IP_H

#include <stdint.h>

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

struct ipv4_header {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t dscp;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint8_t src_ip[4];
    uint8_t dest_ip[4];
} __attribute__((packed));

uint16_t calculate_checksum(const void *data, int count);
void ip_send(const uint8_t *dest_ip, uint8_t protocol, const uint8_t *payload, uint16_t payload_len);
void ip_handle_packet(const uint8_t *packet, uint16_t len);

#endif
