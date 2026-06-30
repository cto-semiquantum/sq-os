#include "ne2000.h"
#include "pci.h"
#include "../include/kernel.h"

uint8_t ne2k_mac[6];
uint32_t ne2k_io_base = 0;
int ne2k_found = 0;
uint32_t ne2k_tx_packets = 0;
uint32_t ne2k_rx_packets = 0;

static uint8_t next_packet_page = 0x47;

int ne2k_init(void) {
    PCIDevice dev;
    // Vendor 0x10EC, Device 0x8029 (Realtek RTL8029 NE2000)
    if (!pci_find_device(0x10EC, 0x8029, &dev)) {
        return 0; // Not found
    }

    ne2k_io_base = dev.bar0 & ~3;
    ne2k_found = 1;

    // Reset the card
    uint8_t rst = inb(ne2k_io_base + NE_RST);
    outb(ne2k_io_base + NE_RST, rst);
    
    // Some small delay
    for (volatile int i = 0; i < 10000; i++);
    
    // Stop mode, Page 0, abort remote DMA
    outb(ne2k_io_base + NE_CR, 0x21); 

    // Word transfer mode, 8-byte FIFO
    outb(ne2k_io_base + NE_DCR, 0x49);

    // Clear remote byte count
    outb(ne2k_io_base + NE_RBCR0, 0x00);
    outb(ne2k_io_base + NE_RBCR1, 0x00);

    // Monitor mode (no packets)
    outb(ne2k_io_base + NE_RCR, 0x20);
    // Internal loopback
    outb(ne2k_io_base + NE_TCR, 0x02);

    // Receive ring 0x46 - 0x80 (16KB RTL8029)
    outb(ne2k_io_base + NE_PSTART, 0x46);
    outb(ne2k_io_base + NE_PSTOP, 0x80);
    outb(ne2k_io_base + NE_BNRY, 0x46);
    
    // Mask all interrupts, clear status
    outb(ne2k_io_base + NE_IMR, 0x00);
    outb(ne2k_io_base + NE_ISR, 0xFF);

    // Read MAC address
    // Start remote read from address 0x0000, 32 bytes
    outb(ne2k_io_base + NE_RSAR0, 0x00);
    outb(ne2k_io_base + NE_RSAR1, 0x00);
    outb(ne2k_io_base + NE_RBCR0, 32);
    outb(ne2k_io_base + NE_RBCR1, 0x00);
    outb(ne2k_io_base + NE_CR, 0x0A); // Page 0, Remote Read, Start

    for (int i = 0; i < 6; i++) {
        ne2k_mac[i] = inw(ne2k_io_base + NE_DATA) & 0xFF;
    }
    // Read the rest to drain
    for (int i = 6; i < 16; i++) {
        inw(ne2k_io_base + NE_DATA);
    }

    // Set MAC and CURR page
    outb(ne2k_io_base + NE_CR, 0x61); // Page 1, Stop
    for (int i = 0; i < 6; i++) {
        outb(ne2k_io_base + NE_PAR0 + i, ne2k_mac[i]);
    }
    outb(ne2k_io_base + NE_CURR, 0x47);
    next_packet_page = 0x47;

    // Start mode
    outb(ne2k_io_base + NE_CR, 0x22); // Page 0, Start
    outb(ne2k_io_base + NE_TCR, 0x00); // Normal TX
    // Accept broadcast and our MAC
    outb(ne2k_io_base + NE_RCR, 0x0F); 

    // Clear ISR and enable interrupts (if using them)
    outb(ne2k_io_base + NE_ISR, 0xFF);
    
    return 1;
}

void ne2k_transmit(const uint8_t *data, uint16_t length) {
    if (!ne2k_found) return;

    if (length < 60) length = 60; // Min ethernet frame size (excluding FCS)

    // Remote Write to 0x4000 (TX buffer page 0x40)
    outb(ne2k_io_base + NE_CR, 0x22); // Page 0, Start, Abort DMA
    outb(ne2k_io_base + NE_ISR, 0x40); // Clear RDC
    outb(ne2k_io_base + NE_RSAR0, 0x00);
    outb(ne2k_io_base + NE_RSAR1, 0x40);
    outb(ne2k_io_base + NE_RBCR0, length & 0xFF);
    outb(ne2k_io_base + NE_RBCR1, length >> 8);
    outb(ne2k_io_base + NE_CR, 0x12); // Remote Write, Start

    const uint16_t *word_data = (const uint16_t *)data;
    for (int i = 0; i < (length + 1) / 2; i++) {
        outw(ne2k_io_base + NE_DATA, word_data[i]);
    }

    // Wait for DMA complete
    while ((inb(ne2k_io_base + NE_ISR) & 0x40) == 0) {}

    // Transmit
    outb(ne2k_io_base + NE_CR, 0x22); // Page 0
    outb(ne2k_io_base + NE_TPSR, 0x40);
    outb(ne2k_io_base + NE_TBCR0, length & 0xFF);
    outb(ne2k_io_base + NE_TBCR1, length >> 8);
    outb(ne2k_io_base + NE_CR, 0x26); // Transmit

    ne2k_tx_packets++;
}

struct ne2k_rx_header {
    uint8_t status;
    uint8_t next_page;
    uint16_t length;
} __attribute__((packed));

uint16_t ne2k_receive(uint8_t *buffer, uint16_t max_len) {
    if (!ne2k_found) return 0;

    outb(ne2k_io_base + NE_CR, 0x62); // Page 1
    uint8_t curr = inb(ne2k_io_base + NE_CURR);
    outb(ne2k_io_base + NE_CR, 0x22); // Page 0

    if (next_packet_page == curr) {
        return 0; // No packets
    }

    // Remote Read the header
    outb(ne2k_io_base + NE_RSAR0, 0x00);
    outb(ne2k_io_base + NE_RSAR1, next_packet_page);
    outb(ne2k_io_base + NE_RBCR0, 4); // 4 byte header
    outb(ne2k_io_base + NE_RBCR1, 0);
    outb(ne2k_io_base + NE_CR, 0x0A); // Remote Read

    struct ne2k_rx_header rx_hdr;
    uint16_t *hdr_words = (uint16_t *)&rx_hdr;
    hdr_words[0] = inw(ne2k_io_base + NE_DATA);
    hdr_words[1] = inw(ne2k_io_base + NE_DATA);

    uint16_t pkt_len = rx_hdr.length - 4; // Subtract header
    if (pkt_len > max_len) pkt_len = max_len;

    // Determine wrap around
    uint16_t page_offset = next_packet_page;
    uint16_t read_ptr = (page_offset << 8) + 4;
    
    uint16_t to_read = pkt_len;
    uint16_t *buf_words = (uint16_t *)buffer;
    int word_idx = 0;

    // Setup DMA read for packet data
    outb(ne2k_io_base + NE_RSAR0, read_ptr & 0xFF);
    outb(ne2k_io_base + NE_RSAR1, read_ptr >> 8);
    outb(ne2k_io_base + NE_RBCR0, to_read & 0xFF);
    outb(ne2k_io_base + NE_RBCR1, to_read >> 8);
    outb(ne2k_io_base + NE_CR, 0x0A); // Remote Read

    for (int i = 0; i < (to_read + 1) / 2; i++) {
        buf_words[word_idx++] = inw(ne2k_io_base + NE_DATA);
    }

    // Note: The RTL8029 doesn't strictly need manual wraparound handling for remote DMA
    // if configured correctly, but we must update the BNRY
    next_packet_page = rx_hdr.next_page;
    uint8_t bnry = next_packet_page - 1;
    if (bnry < 0x46) bnry = 0x7F; // PSTOP - 1
    
    outb(ne2k_io_base + NE_BNRY, bnry);
    ne2k_rx_packets++;

    return pkt_len;
}
