#include "arp.h"
#include "net.h"
#include "ethernet.h"
#include "../drivers/ne2000.h"
#include "../include/kernel.h"

// Simple ARP cache
#define ARP_CACHE_SIZE 16

struct arp_entry {
    uint8_t ip[4];
    uint8_t mac[6];
    int valid;
};

static struct arp_entry arp_cache[ARP_CACHE_SIZE];
static int next_arp_idx = 0;

static void mem_copy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static int ip_eq(const uint8_t *a, const uint8_t *b) {
    return (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}

void arp_init(void) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = 0;
    }
}

static void arp_update_cache(const uint8_t *ip, const uint8_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && ip_eq(arp_cache[i].ip, ip)) {
            mem_copy(arp_cache[i].mac, mac, 6);
            return;
        }
    }
    
    // Add new
    mem_copy(arp_cache[next_arp_idx].ip, ip, 4);
    mem_copy(arp_cache[next_arp_idx].mac, mac, 6);
    arp_cache[next_arp_idx].valid = 1;
    
    next_arp_idx = (next_arp_idx + 1) % ARP_CACHE_SIZE;
}

int arp_lookup(const uint8_t *ip, uint8_t *mac_out) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && ip_eq(arp_cache[i].ip, ip)) {
            mem_copy(mac_out, arp_cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

void arp_request(const uint8_t *target_ip) {
    uint8_t bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    struct arp_header req;
    req.hw_type = htons(1);
    req.proto_type = htons(ETHERTYPE_IPv4);
    req.hw_size = 6;
    req.proto_size = 4;
    req.opcode = htons(ARP_OP_REQUEST);
    
    mem_copy(req.sender_mac, ne2k_mac, 6);
    mem_copy(req.sender_ip, my_ip, 4);
    mem_copy(req.target_mac, bcast_mac, 6); // Or all 0s
    mem_copy(req.target_ip, target_ip, 4);
    
    ethernet_send(bcast_mac, ETHERTYPE_ARP, (const uint8_t*)&req, sizeof(req));
}

void arp_handle_packet(const uint8_t *packet, uint16_t len) {
    if (len < sizeof(struct arp_header)) return;
    
    struct arp_header *arp = (struct arp_header *)packet;
    
    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != ETHERTYPE_IPv4) return;
    
    arp_update_cache(arp->sender_ip, arp->sender_mac);
    
    if (ntohs(arp->opcode) == ARP_OP_REQUEST) {
        if (ip_eq(arp->target_ip, my_ip)) {
            // Send reply
            struct arp_header rep;
            rep.hw_type = htons(1);
            rep.proto_type = htons(ETHERTYPE_IPv4);
            rep.hw_size = 6;
            rep.proto_size = 4;
            rep.opcode = htons(ARP_OP_REPLY);
            
            mem_copy(rep.sender_mac, ne2k_mac, 6);
            mem_copy(rep.sender_ip, my_ip, 4);
            mem_copy(rep.target_mac, arp->sender_mac, 6);
            mem_copy(rep.target_ip, arp->sender_ip, 4);
            
            ethernet_send(arp->sender_mac, ETHERTYPE_ARP, (const uint8_t*)&rep, sizeof(rep));
        }
    }
}

// Just exposing cache directly would be better, but this works for terminal
extern void append_history(const char *line);

// Basic itoa for IPs and MACs
static void btox(uint8_t val, char *out) {
    const char *hex = "0123456789ABCDEF";
    out[0] = hex[val >> 4];
    out[1] = hex[val & 0x0F];
}

static void btoa(uint8_t val, char *out, int *len) {
    if (val == 0) { out[0]='0'; *len=1; return; }
    char tmp[4]; int n = 0;
    while (val > 0) { tmp[n++] = '0' + (val % 10); val /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    *len = n;
}

void arp_print_cache(void) {
    int found = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid) {
            found = 1;
            char line[64];
            int p = 0;
            for(int j=0; j<4; j++) {
                int l;
                btoa(arp_cache[i].ip[j], &line[p], &l);
                p += l;
                if(j<3) line[p++] = '.';
            }
            while(p < 16) line[p++] = ' ';
            line[p++] = '-'; line[p++] = '>'; line[p++] = ' ';
            
            for(int j=0; j<6; j++) {
                btox(arp_cache[i].mac[j], &line[p]);
                p += 2;
                if(j<5) line[p++] = ':';
            }
            line[p] = 0;
            append_history(line);
        }
    }
    if (!found) append_history("ARP cache is empty");
}
