#include "rtc.h"
#include "kernel.h"

static inline uint8_t get_update_in_progress_flag(void) {
    outb(0x70, 0x0A);
    return (inb(0x71) & 0x80);
}

static uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

void read_rtc(int *hours, int *minutes) {
    // Wait until RTC update is NOT in progress
    while (get_update_in_progress_flag());

    uint8_t second = get_rtc_register(0x00);
    uint8_t minute = get_rtc_register(0x02);
    uint8_t hour   = get_rtc_register(0x04);
    uint8_t registerB = get_rtc_register(0x0B);

    // Convert BCD to binary if BCD mode is active (bit 2 is 0)
    if (!(registerB & 0x04)) {
        *minutes = ((minute & 0xF0) >> 4) * 10 + (minute & 0x0F);
        *hours = ((hour & 0xF0) >> 4) * 10 + (hour & 0x0F);
    } else {
        *minutes = minute;
        *hours = hour;
    }

    // Handle 12-hour clock (if bit 1 is 0)
    if (!(registerB & 0x02) && (*hours & 0x80)) {
        *hours = ((*hours & 0x7F) + 12) % 24;
    }
}
