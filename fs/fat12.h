#ifndef FAT12_H
#define FAT12_H

#include <stdint.h>

/* ============================================================
 * FAT12 Filesystem Foundation — SQ-OS
 * ============================================================
 * Implements ATA PIO-28 disk access and FAT12 root directory
 * parsing. Currently wired for QEMU's default primary ATA
 * channel (I/O base 0x1F0).
 *
 * Disk image layout (SQ-OS, 51 sectors):
 *   Sector 0      Boot sector (BPB lives here)
 *   Sector 1–4   FAT1
 *   Sector 5–8   FAT2 (mirror)
 *   Sector 9–22  Root directory (14 sectors × 16 entries = 224 max)
 *   Sector 23+   Data clusters
 * ============================================================ */

/* ATA PIO primary channel I/O ports */
#define ATA_DATA        0x1F0
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_CMD_STATUS  0x1F7
#define ATA_CMD_READ    0x20    /* Read sectors with retry */

/* BPB — BIOS Parameter Block (first 62 bytes of boot sector) */
typedef struct __attribute__((packed)) {
    uint8_t  jump[3];           /* 0x00  Jump instruction       */
    uint8_t  oem[8];            /* 0x03  OEM name               */
    uint16_t bytes_per_sector;  /* 0x0B  Almost always 512      */
    uint8_t  sectors_per_cluster; /* 0x0D                       */
    uint16_t reserved_sectors;  /* 0x0E  Usually 1 (boot sec)   */
    uint8_t  num_fats;          /* 0x10  Usually 2              */
    uint16_t root_entry_count;  /* 0x11  224 for FAT12 floppy   */
    uint16_t total_sectors_16;  /* 0x13                         */
    uint8_t  media_type;        /* 0x15  0xF8=HDD, 0xF0=floppy */
    uint16_t fat_size_16;       /* 0x16  Sectors per FAT        */
    uint16_t sectors_per_track; /* 0x18                         */
    uint16_t num_heads;         /* 0x1A                         */
    uint32_t hidden_sectors;    /* 0x1C                         */
    uint32_t total_sectors_32;  /* 0x20                         */
    /* Extended boot record */
    uint8_t  drive_number;      /* 0x24                         */
    uint8_t  reserved1;         /* 0x25                         */
    uint8_t  boot_sig;          /* 0x26  0x29 = extended        */
    uint32_t volume_id;         /* 0x27                         */
    uint8_t  volume_label[11];  /* 0x2B                         */
    uint8_t  fs_type[8];        /* 0x36  "FAT12   "             */
} BPB;

/* FAT12 8.3 directory entry (32 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  name[8];           /* Filename (space-padded)      */
    uint8_t  ext[3];            /* Extension (space-padded)     */
    uint8_t  attr;              /* File attributes              */
    uint8_t  reserved[10];      /* WinNT / access time fields   */
    uint16_t time;              /* Last write time              */
    uint16_t date;              /* Last write date              */
    uint16_t first_cluster;     /* Starting cluster             */
    uint32_t file_size;         /* File size in bytes           */
} DirEntry;

/* Directory entry attribute flags */
#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F   /* Long file name marker       */

/* Maximum entries returned by fat12_list_root() */
#define FAT12_MAX_FILES     16

/* ============================================================
 * Public API
 * ============================================================ */

/* disk_read_sector — read one 512-byte sector via ATA PIO-28.
 * lba    : logical block address (0-based)
 * buffer : caller-provided 512-byte buffer
 * Returns 0 on success, -1 on timeout / error. */
int disk_read_sector(uint32_t lba, uint8_t *buffer);

/* fat12_init — read and validate the boot sector BPB.
 * Returns 0 if a valid FAT12 volume signature is found, -1 otherwise. */
int fat12_init(void);

/* fat12_list_root — populate out_entries with up to max_entries directory
 * entries from the root directory.
 * Returns the number of valid (non-deleted, non-empty) entries found. */
int fat12_list_root(DirEntry *out_entries, int max_entries);

/* fat12_find_file — search the root directory for a file by name.
 * Returns 0 on success, -1 if not found. */
int fat12_find_file(const char *name, DirEntry *out_entry);

/* fat12_read_file — read file content contiguous blocks.
 * Returns number of bytes read, or -1 on error. */
int fat12_read_file(DirEntry *entry, uint8_t *buffer, uint32_t max_bytes);

/* disk_write_sector — write one 512-byte sector via ATA PIO-28.
 * Returns 0 on success, -1 on error. */
int disk_write_sector(uint32_t lba, const uint8_t *buffer);

/* fs_create_file — search or create a file in the root directory.
 * Returns 0 on success, -1 on error. */
int fs_create_file(const char *name, uint16_t first_cluster);

/* fs_write_file — write/overwrite a file's contents and update directory entry size.
 * Returns number of bytes written, or -1 on error. */
int fs_write_file(const char *name, const uint8_t *buffer, uint32_t size);

/* fs_read_file — open and read a file's contents into a buffer.
 * Returns number of bytes read, or -1 on error. */
int fs_read_file(const char *name, uint8_t *buffer, uint32_t max_bytes);

/* fs_delete_file — delete a file in the root directory by marking name[0] = 0xE5.
 * Returns 0 on success, -1 on error/not found. */
int fs_delete_file(const char *name);

/* Cached BPB from boot sector (available after fat12_init() succeeds). */
extern BPB g_bpb;

#endif /* FAT12_H */
