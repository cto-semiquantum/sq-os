#include "ethernet.h"
#include "net.h"
#include "arp.h"
#include "ip.h"
#include "../drivers/ne2000.h"

// Basic memory copy since we don't have libc
static void mem_copy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static uint8_t tx_buffer[2048];

void ethernet_send(const uint8_t *dest_mac, uint16_t ethertype, const uint8_t *payload, uint16_t payload_len) {
    struct ethernet_header *hdr = (struct ethernet_header *)tx_buffer;
    
    mem_copy(hdr->dest_mac, dest_mac, 6);
    mem_copy(hdr->src_mac, ne2k_mac, 6);
    hdr->ethertype = htons(ethertype);
    
    mem_copy(tx_buffer + sizeof(struct ethernet_header), payload, payload_len);
    
    uint16_t total_len = sizeof(struct ethernet_header) + payload_len;
    ne2k_transmit(tx_buffer, total_len);
}

void ethernet_receive(const uint8_t *buffer, uint16_t len) {
    if (len < sizeof(struct ethernet_header)) return;
    
    struct ethernet_header *hdr = (struct ethernet_header *)buffer;
    uint16_t type = ntohs(hdr->ethertype);
    
    uint16_t payload_len = len - sizeof(struct ethernet_header);
    const uint8_t *payload = buffer + sizeof(struct ethernet_header);
    
    if (type == ETHERTYPE_ARP) {
        arp_handle_packet(payload, payload_len);
    } else if (type == ETHERTYPE_IPv4) {
        ip_handle_packet(payload, payload_len);
    }
}
