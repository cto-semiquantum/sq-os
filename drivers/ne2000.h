#ifndef NE2000_H
#define NE2000_H

#include <stdint.h>

#define NE_CR       0x00
#define NE_PSTART   0x01
#define NE_PSTOP    0x02
#define NE_BNRY     0x03
#define NE_TPSR     0x04
#define NE_TBCR0    0x05
#define NE_TBCR1    0x06
#define NE_ISR      0x07
#define NE_RSAR0    0x08
#define NE_RSAR1    0x09
#define NE_RBCR0    0x0A
#define NE_RBCR1    0x0B
#define NE_RSR      0x0C
#define NE_RCR      0x0A // Page 0 write is RCR, read is RBCR0? Wait, Page 0 write to 0x0A is RBCR0. Wait, no. RBCR0 is 0x0A, RCR is 0x0C write?
// Actually:
// Page 0 Write:
// 01: PSTART
// 02: PSTOP
// 03: BNRY
// 04: TPSR
// 05: TBCR0
// 06: TBCR1
// 07: ISR
// 08: RSAR0
// 09: RSAR1
// 0A: RBCR0
// 0B: RBCR1
// 0C: RCR
// 0D: TCR
// 0E: DCR
// 0F: IMR

#define NE_TCR      0x0D
#define NE_DCR      0x0E
#define NE_IMR      0x0F
#define NE_DATA     0x10
#define NE_RST      0x1F

// Page 1 Read/Write
#define NE_PAR0     0x01
#define NE_CURR     0x07

// MAC address
extern uint8_t ne2k_mac[6];
extern uint32_t ne2k_io_base;
extern int ne2k_found;
extern uint32_t ne2k_tx_packets;
extern uint32_t ne2k_rx_packets;

int ne2k_init(void);
void ne2k_transmit(const uint8_t *data, uint16_t length);
uint16_t ne2k_receive(uint8_t *buffer, uint16_t max_len);

#endif
