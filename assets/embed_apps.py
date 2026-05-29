#!/usr/bin/env python3
"""
embed_apps.py — SQ-OS App Store Builder
========================================
Writes the app directory (sector 50) and app binaries (sector 51+)
into os.img after the kernel has been linked.

Disk layout managed by this script:

  Sector 50        App directory  (8 × 64-byte entries = 512 bytes)
  Sector 51        hello.bin      (flat 32-bit PIC binary)
  Sector 52+       (future apps)

App directory entry (64 bytes, little-endian):
  [0:12]   char[12]   Filename, null-padded  e.g. b"HELLO.APP\\x00\\x00\\x00"
  [12:16]  uint32_t   First sector on disk
  [16:20]  uint32_t   Binary size in bytes
  [20:64]  uint8_t[44] Reserved (zeros)

Binary sectors:
  Raw binary data, no header prefix.
  The size is stored in the app directory entry.
"""

import struct, os, sys

SECTOR_SIZE       = 512
APP_DIR_SECTOR    = 80
FIRST_APP_SECTOR  = 81   # hello.app starts here


def pad_sector(data: bytes) -> bytes:
    """Pad data to a multiple of SECTOR_SIZE with zeros."""
    rem = len(data) % SECTOR_SIZE
    if rem:
        data += bytes(SECTOR_SIZE - rem)
    return data


def make_dir_entry(name: str, ext: str, attr: int, first_cluster: int, size: int) -> bytes:
    name_bytes = name.encode('ascii')[:8].ljust(8, b' ')
    ext_bytes = ext.encode('ascii')[:3].ljust(3, b' ')
    return struct.pack('<8s3sB10sHHHI', name_bytes, ext_bytes, attr, b'\x00'*10, 0, 0, first_cluster, size)

def embed(disk_path: str, apps: list[tuple[str, str]]) -> None:
    """
    apps: list of (filename, binary_path) tuples.
    filename: 8.3 ASCII name, e.g. "HELLO.APP"
    """
    # Read disk
    with open(disk_path, 'rb') as f:
        disk = bytearray(f.read())

    # Ensure disk covers at least the directory sector + all app sectors
    cur_sector = FIRST_APP_SECTOR
    app_binaries = []
    for (fname, fpath) in apps:
        with open(fpath, 'rb') as f:
            data = f.read()
        app_binaries.append((fname, cur_sector, data))
        cur_sector += (len(data) + SECTOR_SIZE - 1) // SECTOR_SIZE

    # Ensure disk is large enough (at least up to sector 96 to cover FAT12 structure)
    needed = max(cur_sector * SECTOR_SIZE, 96 * SECTOR_SIZE)
    if len(disk) < needed:
        disk += bytearray(needed - len(disk))

    # ---- Build app directory (sector 80) ----
    dir_sector = bytearray(SECTOR_SIZE)
    for idx, (fname, sector, data) in enumerate(app_binaries):
        if idx >= 8:
            print(f"[embed_apps] WARNING: max 8 apps, skipping {fname}")
            break
        # Encode filename: up to 12 bytes, null-padded
        name_bytes = fname.encode('ascii')[:12].ljust(12, b'\x00')
        entry = struct.pack('<12sII', name_bytes, sector, len(data))
        entry += bytes(64 - len(entry))  # pad to 64 bytes
        dir_sector[idx * 64 : (idx + 1) * 64] = entry

    # Write directory sector
    dir_off = APP_DIR_SECTOR * SECTOR_SIZE
    disk[dir_off : dir_off + SECTOR_SIZE] = dir_sector

    # ---- Write each binary (for loader.c starting at sector 81) ----
    for (fname, sector, data) in app_binaries:
        padded = pad_sector(data)
        off = sector * SECTOR_SIZE
        disk[off : off + len(padded)] = padded
        print(f"[embed_apps] {fname:12s}  sector={sector}  size={len(data)} bytes")

    # ---- Write FAT12 filesystem structures (FATs starting at sector 82, Root at sector 88) ----
    fat_size = 3 * SECTOR_SIZE
    root_size = 4 * SECTOR_SIZE
    
    hello_size = len(app_binaries[0][2]) if len(app_binaries) > 0 else 0
    notes_content = b"Welcome to SQ Notes!\nCreate notes here.\nPress SAVE to save."
    readme_content = b"=== SQ-OS README ===\nWelcome to Antigravity OS!\nEnjoy the Task Manager,\nCalculator, Notes, and Settings.\nSQ-OS rocks!"
    
    e1 = make_dir_entry("HELLO", "APP", 0x20, 2, hello_size)
    e2 = make_dir_entry("NOTES", "TXT", 0x20, 3, len(notes_content))
    e3 = make_dir_entry("README", "TXT", 0x20, 4, len(readme_content))
    
    root_sector = bytearray(root_size)
    root_sector[0:32] = e1
    root_sector[32:64] = e2
    root_sector[64:96] = e3
    
    # Write empty/dummy FAT1 and FAT2
    disk[82 * SECTOR_SIZE : 85 * SECTOR_SIZE] = bytearray(fat_size)
    disk[85 * SECTOR_SIZE : 88 * SECTOR_SIZE] = bytearray(fat_size)
    # Write Root directory
    disk[88 * SECTOR_SIZE : 92 * SECTOR_SIZE] = root_sector
    
    # Write file data clusters starting at LBA 92
    # Cluster 2 (LBA 92): hello.bin contents
    if hello_size > 0:
        disk[92 * SECTOR_SIZE : 93 * SECTOR_SIZE] = pad_sector(app_binaries[0][2])
    # Cluster 3 (LBA 93): NOTES.TXT contents
    disk[93 * SECTOR_SIZE : 94 * SECTOR_SIZE] = pad_sector(notes_content)
    # Cluster 4 (LBA 94): README.TXT contents
    disk[94 * SECTOR_SIZE : 95 * SECTOR_SIZE] = pad_sector(readme_content)

    # Write back
    with open(disk_path, 'wb') as f:
        f.write(bytes(disk))

    print(f"[embed_apps] Done. App directory at sector {APP_DIR_SECTOR}. FAT12 structures at sectors 82-94.")


if __name__ == '__main__':
    # Apps to embed: list of (DISK_NAME, local_binary_path)
    apps = [
        ("HELLO.APP", "hello.bin"),
    ]

    disk_path = 'os.img'
    if not os.path.exists(disk_path):
        print(f"[embed_apps] ERROR: {disk_path} not found — run build.bat first")
        sys.exit(1)

    embed(disk_path, apps)
