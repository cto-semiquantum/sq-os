#include "ip.h"
#include "net.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "tcp.h"
#include "../include/kernel.h"

static uint16_t ip_id = 0;

static void mem_copy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static int ip_eq(const uint8_t *a, const uint8_t *b) {
    return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

uint16_t calculate_checksum(const void *data, int count) {
    register uint32_t sum = 0;
    const uint16_t *ptr = data;

    while (count > 1) {
        sum += *ptr++;
        count -= 2;
    }

    if (count > 0) {
        sum += *(uint8_t *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

static uint8_t ip_tx_buffer[2048];

void ip_send(const uint8_t *dest_ip, uint8_t protocol, const uint8_t *payload, uint16_t payload_len) {
    struct ipv4_header *hdr = (struct ipv4_header *)ip_tx_buffer;
    
    hdr->version = 4;
    hdr->ihl = 5;
    hdr->dscp = 0;
    hdr->total_length = htons(sizeof(struct ipv4_header) + payload_len);
    hdr->identification = htons(ip_id++);
    hdr->flags_frag = 0;
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->header_checksum = 0;
    
    mem_copy(hdr->src_ip, my_ip, 4);
    mem_copy(hdr->dest_ip, dest_ip, 4);
    
    hdr->header_checksum = calculate_checksum(hdr, sizeof(struct ipv4_header));
    
    mem_copy(ip_tx_buffer + sizeof(struct ipv4_header), payload, payload_len);
    
    // Resolve MAC
    uint8_t dest_mac[6];
    if (arp_lookup(dest_ip, dest_mac)) {
        ethernet_send(dest_mac, ETHERTYPE_IPv4, ip_tx_buffer, sizeof(struct ipv4_header) + payload_len);
    } else {
        // Broadcast ARP and hope for the best for next time?
        // Let's do a simple blocking ARP resolution for simplicity in this OS
        // Or we send it to gateway
        // For simplicity, we just send ARP request if not found, packet is dropped this time
        arp_request(dest_ip);
    }
}

void ip_handle_packet(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(struct ipv4_header)) return;
    
    struct ipv4_header *hdr = (struct ipv4_header *)packet;
    
    if (hdr->version != 4) return;
    
    uint16_t header_len = hdr->ihl * 4;
    if (len < header_len) return;
    
    if (!ip_eq(hdr->dest_ip, my_ip) && hdr->dest_ip[3] != 255) return;
    
    uint16_t payload_len = ntohs(hdr->total_length) - header_len;
    const uint8_t *payload = packet + header_len;
    
    if (hdr->protocol == IP_PROTO_ICMP) {
        icmp_handle_packet(hdr->src_ip, payload, payload_len);
    } else if (hdr->protocol == IP_PROTO_TCP) {
        tcp_handle_packet(hdr->src_ip, payload, payload_len);
    }
}
