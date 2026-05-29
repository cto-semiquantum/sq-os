#include "../fs/fat12.h"
#include "kernel.h"    /* inb / outb / inw / outw helpers */

/* ============================================================
 * Cached BPB (populated by fat12_init)
 * ============================================================ */
BPB g_bpb;

/* ============================================================
 * ATA PIO Helper — wait until drive is ready (BSY clear, DRQ set)
 * ============================================================ */
static int ata_wait_ready(void) {
    /* Timeout: ~65535 polling iterations (~a few milliseconds in QEMU) */
    for (int i = 0; i < 0xFFFF; i++) {
        uint8_t status = inb(ATA_CMD_STATUS);
        if (status & 0x01) return -1;  /* ERR bit set */
        if (status & 0x08) return 0;   /* DRQ set — data ready */
        if (!(status & 0x80)) {
            /* BSY clear — check DRQ one more time */
            if (status & 0x08) return 0;
        }
    }
    return -1; /* Timeout */
}

/* ============================================================
 * disk_read_sector — ATA PIO 28-bit LBA single-sector read
 *
 * LBA register layout:
 *   0x1F6[7]   = 1 (must be set)
 *   0x1F6[6]   = 1 (LBA mode)
 *   0x1F6[5]   = 1 (must be set)
 *   0x1F6[4]   = 0 (master drive)
 *   0x1F6[3:0] = LBA[27:24]
 *   0x1F5      = LBA[23:16]
 *   0x1F4      = LBA[15:8]
 *   0x1F3      = LBA[7:0]
 *   0x1F2      = sector count (1)
 *   0x1F7      = 0x20 (READ SECTORS WITH RETRY)
 * ============================================================ */
int disk_read_sector(uint32_t lba, uint8_t *buffer) {
    /* Wait until drive is not busy */
    for (int i = 0; i < 0xFFFF; i++) {
        if (!(inb(ATA_CMD_STATUS) & 0x80)) break;
    }

    /* Send LBA + sector count */
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)( lba        & 0xFF));
    outb(ATA_LBA_MID,  (uint8_t)((lba >> 8)  & 0xFF));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));

    /* Issue read command */
    outb(ATA_CMD_STATUS, ATA_CMD_READ);

    /* Wait for data */
    if (ata_wait_ready() != 0) return -1;

    /* Read 256 words (= 512 bytes) from the data port */
    uint16_t *buf16 = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        buf16[i] = inw(ATA_DATA);
    }

    return 0; /* Success */
}

/* ============================================================
 * fat12_init — read boot sector and validate BPB
 * ============================================================ */
int fat12_init(void) {
    uint8_t sector[512];

    if (disk_read_sector(0, sector) != 0) {
        return -1; /* Disk read failed */
    }

    /* Copy BPB from sector buffer */
    uint8_t *bpb_raw = (uint8_t *)&g_bpb;
    for (int i = 0; i < (int)sizeof(BPB); i++) {
        bpb_raw[i] = sector[i];
    }

    /* Validate boot signature: bytes 510-511 must be 0x55 0xAA */
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return -1;
    }

    return 0; /* Valid FAT12 volume */
}

/* ============================================================
 * fat12_list_root — list files in the root directory
 *
 * Root directory location (FAT12):
 *   First root sector = reserved_sectors + (num_fats * fat_size_16)
 *   Root sector count = (root_entry_count * 32) / bytes_per_sector
 *
 * For our disk image (defaults: 1 reserved, 2 FATs, 9 FAT sectors each):
 *   Root start LBA = 1 + (2 × 9) = 19
 *   Root sectors   = (224 × 32) / 512 = 14
 * ============================================================ */
int fat12_list_root(DirEntry *out_entries, int max_entries) {
    /* Compute root directory location from BPB */
    uint32_t root_lba = (uint32_t)g_bpb.reserved_sectors
                      + ((uint32_t)g_bpb.num_fats * (uint32_t)g_bpb.fat_size_16);

    uint32_t root_size_bytes = (uint32_t)g_bpb.root_entry_count * 32U;
    uint32_t root_sectors    = root_size_bytes / (uint32_t)g_bpb.bytes_per_sector;

    int found = 0;
    uint8_t sector[512];

    for (uint32_t s = 0; s < root_sectors && found < max_entries; s++) {
        if (disk_read_sector(root_lba + s, sector) != 0) break;

        /* Each sector holds bytes_per_sector / 32 entries */
        int entries_per_sector = (int)(g_bpb.bytes_per_sector / 32);

        for (int e = 0; e < entries_per_sector && found < max_entries; e++) {
            DirEntry *entry = (DirEntry *)(sector + e * 32);

            /* 0x00 = no more entries; 0xE5 = deleted */
            if (entry->name[0] == 0x00) goto done;
            if (entry->name[0] == 0xE5) continue;

            /* Skip volume labels and LFN entries */
            if (entry->attr & FAT_ATTR_VOLUME_ID) continue;
            if (entry->attr == FAT_ATTR_LFN)      continue;

            /* Copy to output array */
            uint8_t *dst = (uint8_t *)&out_entries[found];
            uint8_t *src = (uint8_t *)entry;
            for (int i = 0; i < 32; i++) dst[i] = src[i];
            found++;
        }
    }

done:
    return found;
}
