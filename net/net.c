#include "net.h"
#include "../drivers/ne2000.h"
#include "ethernet.h"
#include "arp.h"
#include "tcp.h"

uint8_t my_ip[4] = {10, 0, 2, 15}; // QEMU user net default
uint8_t gateway_ip[4] = {10, 0, 2, 2};
uint8_t netmask[4] = {255, 255, 255, 0};

void net_init(void) {
    ne2k_init();
    arp_init();
    tcp_init();
}

static uint8_t rx_buffer[2048];

void net_poll(void) {
    uint16_t len;
    while ((len = ne2k_receive(rx_buffer, sizeof(rx_buffer))) > 0) {
        ethernet_receive(rx_buffer, len);
    }
}
