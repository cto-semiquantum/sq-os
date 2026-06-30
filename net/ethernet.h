#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>

#define ETHERTYPE_IPv4 0x0800
#define ETHERTYPE_ARP  0x0806

struct ethernet_header {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
} __attribute__((packed));

void ethernet_send(const uint8_t *dest_mac, uint16_t ethertype, const uint8_t *payload, uint16_t payload_len);
void ethernet_receive(const uint8_t *buffer, uint16_t len);

#endif
