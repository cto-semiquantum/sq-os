#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

struct arp_header {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t hw_size;
    uint8_t proto_size;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
} __attribute__((packed));

void arp_init(void);
void arp_request(const uint8_t *target_ip);
void arp_handle_packet(const uint8_t *packet, uint16_t len);
int arp_lookup(const uint8_t *ip, uint8_t *mac_out);
void arp_print_cache(void);

#endif
