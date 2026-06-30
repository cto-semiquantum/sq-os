#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "../include/kernel.h"

volatile int icmp_reply_received = 0;
volatile uint32_t icmp_reply_ms = 0;
extern volatile uint32_t system_ticks;

static void mem_copy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

void icmp_send_echo_request(const uint8_t *dest_ip, uint16_t id, uint16_t seq) {
    uint8_t packet[64];
    struct icmp_header *hdr = (struct icmp_header *)packet;
    
    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->identifier = htons(id);
    hdr->sequence = htons(seq);
    
    // Fill payload
    for (int i = sizeof(struct icmp_header); i < 64; i++) {
        packet[i] = 'A' + (i % 26);
    }
    
    hdr->checksum = calculate_checksum(packet, 64);
    
    icmp_reply_received = 0;
    
    ip_send(dest_ip, IP_PROTO_ICMP, packet, 64);
}

void icmp_handle_packet(const uint8_t *src_ip, const uint8_t *packet, uint16_t len) {
    if (len < sizeof(struct icmp_header)) return;
    
    struct icmp_header *hdr = (struct icmp_header *)packet;
    
    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        // Send reply
        uint8_t reply[128];
        if (len > 128) len = 128;
        
        mem_copy(reply, packet, len);
        struct icmp_header *rep = (struct icmp_header *)reply;
        rep->type = ICMP_TYPE_ECHO_REPLY;
        rep->checksum = 0;
        rep->checksum = calculate_checksum(reply, len);
        
        ip_send(src_ip, IP_PROTO_ICMP, reply, len);
    } else if (hdr->type == ICMP_TYPE_ECHO_REPLY) {
        icmp_reply_received = 1;
        icmp_reply_ms = system_ticks; // For RTT calculation if needed
    }
}
