#include "pci.h"
#include "../include/kernel.h"

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write out the address
    outl(PCI_CONFIG_ADDRESS, address);

    // Read in the data
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    return (uint16_t)((dword >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_config_read_dword(bus, slot, func, offset);
    return (uint8_t)((dword >> ((offset & 3) * 8)) & 0xFF);
}

int pci_find_device(uint16_t vendor_id, uint16_t device_id, PCIDevice *dev_out) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t ven = pci_config_read_word(bus, slot, func, 0);
                if (ven == 0xFFFF) {
                    if (func == 0) break; // Device doesn't exist
                    continue;
                }
                
                uint16_t dev = pci_config_read_word(bus, slot, func, 2);
                if (ven == vendor_id && dev == device_id) {
                    dev_out->bus = bus;
                    dev_out->device = slot;
                    dev_out->function = func;
                    dev_out->vendor_id = ven;
                    dev_out->device_id = dev;
                    
                    // Read IRQ from offset 0x3C
                    dev_out->irq = pci_config_read_byte(bus, slot, func, 0x3C);
                    
                    // Read BAR0 (I/O base)
                    dev_out->bar0 = pci_config_read_dword(bus, slot, func, 0x10);
                    
                    return 1; // Found
                }
            }
        }
    }
    return 0; // Not found
}
