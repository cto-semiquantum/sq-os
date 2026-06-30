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

int fat12_find_file(const char *name, DirEntry *out_entry) {
    DirEntry entries[FAT12_MAX_FILES];
    int count = fat12_list_root(entries, FAT12_MAX_FILES);
    if (count <= 0) return -1;

    // Convert target name to space-padded 8.3 FAT format (all uppercase)
    char target_name[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char target_ext[3]  = {' ', ' ', ' '};

    int dot_idx = -1;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') {
            dot_idx = i;
            break;
        }
    }

    int name_len = (dot_idx != -1) ? dot_idx : 0;
    if (dot_idx == -1) {
        int l = 0; while (name[l]) l++;
        name_len = l;
    }

    if (name_len > 8) name_len = 8;
    for (int i = 0; i < name_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        target_name[i] = c;
    }

    if (dot_idx != -1) {
        for (int i = 0; i < 3 && name[dot_idx + 1 + i]; i++) {
            char c = name[dot_idx + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            target_ext[i] = c;
        }
    }

    for (int i = 0; i < count; i++) {
        int match = 1;
        for (int k = 0; k < 8; k++) {
            if (entries[i].name[k] != target_name[k]) { match = 0; break; }
        }
        if (match) {
            for (int k = 0; k < 3; k++) {
                if (entries[i].ext[k] != target_ext[k]) { match = 0; break; }
            }
        }
        if (match) {
            if (out_entry) {
                uint8_t *dst = (uint8_t *)out_entry;
                uint8_t *src = (uint8_t *)&entries[i];
                for (int j = 0; j < 32; j++) dst[j] = src[j];
            }
            return 0;
        }
    }

    return -1;
}

int fat12_read_file(DirEntry *entry, uint8_t *buffer, uint32_t max_bytes) {
    if (!entry || !buffer) return -1;
    uint32_t size = entry->file_size;
    if (size > max_bytes) size = max_bytes;

    uint32_t cluster = entry->first_cluster;
    if (cluster < 2) return -1;

    uint32_t bytes_read = 0;
    uint8_t sector_buf[512];

    uint32_t root_sectors = ((uint32_t)g_bpb.root_entry_count * 32U) / (uint32_t)g_bpb.bytes_per_sector;
    uint32_t data_start_lba = (uint32_t)g_bpb.reserved_sectors
                            + ((uint32_t)g_bpb.num_fats * (uint32_t)g_bpb.fat_size_16)
                            + root_sectors;

    while (bytes_read < size) {
        uint32_t lba = data_start_lba + (cluster - 2);
        if (disk_read_sector(lba, sector_buf) != 0) {
            return -1;
        }

        uint32_t chunk = size - bytes_read;
        if (chunk > 512) chunk = 512;

        for (uint32_t i = 0; i < chunk; i++) {
            buffer[bytes_read + i] = sector_buf[i];
        }

        bytes_read += chunk;
        cluster++;
    }

    return (int)bytes_read;
}

#define ATA_CMD_WRITE 0x30

int disk_write_sector(uint32_t lba, const uint8_t *buffer) {
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

    /* Issue write command */
    outb(ATA_CMD_STATUS, ATA_CMD_WRITE);

    /* Wait for DRQ or ready to write */
    for (int i = 0; i < 0xFFFF; i++) {
        uint8_t status = inb(ATA_CMD_STATUS);
        if (status & 0x08) break; // DRQ set — ready
        if (status & 0x01) return -1; // ERR set
    }

    /* Write 256 words (= 512 bytes) to the data port */
    const uint16_t *buf16 = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, buf16[i]);
    }

    /* Wait for write to complete (BSY clear) */
    for (int i = 0; i < 0xFFFF; i++) {
        if (!(inb(ATA_CMD_STATUS) & 0x80)) break;
    }

    return 0; /* Success */
}

int fs_create_file(const char *name, uint16_t first_cluster) {
    if (fat12_init() != 0) return -1;

    DirEntry entry;
    if (fat12_find_file(name, &entry) == 0) {
        return 0; // Already exists
    }

    // Convert target name to space-padded 8.3 FAT format (all uppercase)
    char target_name[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char target_ext[3]  = {' ', ' ', ' '};

    int dot_idx = -1;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') {
            dot_idx = i;
            break;
        }
    }

    int name_len = (dot_idx != -1) ? dot_idx : 0;
    if (dot_idx == -1) {
        int l = 0; while (name[l]) l++;
        name_len = l;
    }

    if (name_len > 8) name_len = 8;
    for (int i = 0; i < name_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        target_name[i] = c;
    }

    if (dot_idx != -1) {
        for (int i = 0; i < 3 && name[dot_idx + 1 + i]; i++) {
            char c = name[dot_idx + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            target_ext[i] = c;
        }
    }

    // Find root directory sector location
    uint32_t root_lba = (uint32_t)g_bpb.reserved_sectors
                      + ((uint32_t)g_bpb.num_fats * (uint32_t)g_bpb.fat_size_16);

    uint32_t root_sectors = ((uint32_t)g_bpb.root_entry_count * 32U) / (uint32_t)g_bpb.bytes_per_sector;
    uint8_t sector[512];

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (disk_read_sector(root_lba + s, sector) != 0) return -1;

        int entries_per_sector = (int)(g_bpb.bytes_per_sector / 32);
        for (int e = 0; e < entries_per_sector; e++) {
            DirEntry *item = (DirEntry *)(sector + e * 32);

            // Empty (0x00) or deleted (0xE5) entry can be reused
            if (item->name[0] == 0x00 || item->name[0] == 0xE5) {
                // Populate the entry
                for (int k = 0; k < 8; k++) item->name[k] = target_name[k];
                for (int k = 0; k < 3; k++) item->ext[k] = target_ext[k];
                item->attr = 0x20; // Archive attribute
                for (int k = 0; k < 10; k++) item->reserved[k] = 0;
                item->time = 0;
                item->date = 0;
                item->first_cluster = first_cluster;
                item->file_size = 0;

                // Write the updated sector back to disk!
                if (disk_write_sector(root_lba + s, sector) != 0) {
                    return -1;
                }
                return 0; // Successfully created
            }
        }
    }

    return -1;
}

int fs_write_file(const char *name, const uint8_t *buffer, uint32_t size) {
    if (fat12_init() != 0) return -1;

    DirEntry entry;
    uint16_t cluster = 3; // Default first cluster (LBA 93) for NOTES.TXT
    if (fat12_find_file(name, &entry) == 0) {
        cluster = entry.first_cluster;
    } else {
        if (fs_create_file(name, cluster) != 0) {
            return -1;
        }
    }

    uint32_t bytes_written = 0;
    uint8_t sector_buf[512];

    uint32_t root_sectors = ((uint32_t)g_bpb.root_entry_count * 32U) / (uint32_t)g_bpb.bytes_per_sector;
    uint32_t data_start_lba = (uint32_t)g_bpb.reserved_sectors
                            + ((uint32_t)g_bpb.num_fats * (uint32_t)g_bpb.fat_size_16)
                            + root_sectors;

    while (bytes_written < size) {
        uint32_t lba = data_start_lba + (cluster - 2);

        for (int i = 0; i < 512; i++) sector_buf[i] = 0;

        uint32_t chunk = size - bytes_written;
        if (chunk > 512) chunk = 512;

        for (uint32_t i = 0; i < chunk; i++) {
            sector_buf[i] = buffer[bytes_written + i];
        }

        if (disk_write_sector(lba, sector_buf) != 0) {
            return -1;
        }

        bytes_written += chunk;
        cluster++;
    }

    uint32_t root_lba = (uint32_t)g_bpb.reserved_sectors
                      + ((uint32_t)g_bpb.num_fats * (uint32_t)g_bpb.fat_size_16);

    char target_name[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char target_ext[3]  = {' ', ' ', ' '};
    int dot_idx = -1;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') { dot_idx = i; break; }
    }
    int name_len = (dot_idx != -1) ? dot_idx : 0;
    if (dot_idx == -1) {
        int l = 0; while (name[l]) l++;
        name_len = l;
    }
    if (name_len > 8) name_len = 8;
    for (int i = 0; i < name_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        target_name[i] = c;
    }
    if (dot_idx != -1) {
        for (int i = 0; i < 3 && name[dot_idx + 1 + i]; i++) {
            char c = name[dot_idx + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            target_ext[i] = c;
        }
    }

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (disk_read_sector(root_lba + s, sector_buf) != 0) return -1;

        int entries_per_sector = (int)(g_bpb.bytes_per_sector / 32);
        for (int e = 0; e < entries_per_sector; e++) {
            DirEntry *item = (DirEntry *)(sector_buf + e * 32);

            int match = 1;
            for (int k = 0; k < 8; k++) {
                if (item->name[k] != target_name[k]) { match = 0; break; }
            }
            if (match) {
                for (int k = 0; k < 3; k++) {
                    if (item->ext[k] != target_ext[k]) { match = 0; break; }
                }
            }
            if (match) {
                item->file_size = size;
                if (disk_write_sector(root_lba + s, sector_buf) != 0) {
                    return -1;
                }
                return (int)bytes_written;
            }
        }
    }

    return -1;
}

int fs_read_file(const char *name, uint8_t *buffer, uint32_t max_bytes) {
    if (fat12_init() != 0) return -1;
    DirEntry entry;
    if (fat12_find_file(name, &entry) != 0) {
        return -1;
    }
    return fat12_read_file(&entry, buffer, max_bytes);
}

int fs_delete_file(const char *name) {
    if (fat12_init() != 0) return -1;

    /* Convert target name to space-padded 8.3 FAT format (all uppercase) */
    char target_name[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char target_ext[3]  = {' ', ' ', ' '};

    int dot_idx = -1;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') { dot_idx = i; break; }
    }

    int name_len = (dot_idx != -1) ? dot_idx : 0;
    if (dot_idx == -1) {
        int l = 0; while (name[l]) l++;
        name_len = l;
    }
    if (name_len > 8) name_len = 8;
    for (int i = 0; i < name_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        target_name[i] = c;
    }
    if (dot_idx != -1) {
        for (int i = 0; i < 3 && name[dot_idx + 1 + i]; i++) {
            char c = name[dot_idx + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            target_ext[i] = c;
        }
    }

    uint32_t root_lba = (uint32_t)g_bpb.reserved_sectors
                      + ((uint32_t)g_bpb.num_fats * (uint32_t)g_bpb.fat_size_16);
    uint32_t root_sectors = ((uint32_t)g_bpb.root_entry_count * 32U) / (uint32_t)g_bpb.bytes_per_sector;

    uint8_t sector[512];
    int entries_per_sector = (int)(g_bpb.bytes_per_sector / 32);

    for (uint32_t s = 0; s < root_sectors; s++) {
        if (disk_read_sector(root_lba + s, sector) != 0) return -1;

        int modified = 0;
        for (int e = 0; e < entries_per_sector; e++) {
            DirEntry *entry = (DirEntry *)(sector + e * 32);
            if (entry->name[0] == 0x00) break;
            if (entry->name[0] == 0xE5) continue;

            int match = 1;
            for (int k = 0; k < 8; k++) {
                if (entry->name[k] != target_name[k]) { match = 0; break; }
            }
            if (match) {
                for (int k = 0; k < 3; k++) {
                    if (entry->ext[k] != target_ext[k]) { match = 0; break; }
                }
            }

            if (match) {
                entry->name[0] = 0xE5; /* Mark as deleted */
                modified = 1;
            }
        }

        if (modified) {
            if (disk_write_sector(root_lba + s, sector) != 0) return -1;
            return 0; /* Success */
        }
    }

    return -1; /* Not found */
}
