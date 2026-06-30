#!/usr/bin/env python3
"""
embed_apps.py — SQ-OS App Store Builder
========================================
Writes the app directory (sector 200) and app binaries (sector 201+)
into os.img after the kernel has been linked.
Also writes FAT12 structures at sector 500+ dynamically.
"""

import struct, os, sys

SECTOR_SIZE       = 512
APP_DIR_SECTOR    = 200
FIRST_APP_SECTOR  = 201   # Apps start here (safe area up to 499)
RESERVED_SECTORS  = 500   # FAT12 filesystem starts at LBA 500


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
    filename: 8.3 ASCII name, e.g. "HELLO.ELF"
    """
    # Read disk
    with open(disk_path, 'rb') as f:
        disk = bytearray(f.read())

    # Ensure disk covers at least up to sector 2880 (1.44MB)
    needed = 2880 * SECTOR_SIZE
    if len(disk) < needed:
        disk += bytearray(needed - len(disk))

    # Read apps and build app directory records
    cur_sector = FIRST_APP_SECTOR
    app_binaries = []
    for (fname, fpath) in apps:
        with open(fpath, 'rb') as f:
            data = f.read()
        app_binaries.append((fname, cur_sector, data))
        cur_sector += (len(data) + SECTOR_SIZE - 1) // SECTOR_SIZE

    if cur_sector > RESERVED_SECTORS:
        print(f"[embed_apps] ERROR: Apps overflow reserved sectors ({cur_sector} > {RESERVED_SECTORS})")
        sys.exit(1)

    # ---- Build and write app directory (sector 200) ----
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

    # ---- Write each app binary into its raw sectors ----
    for (fname, sector, data) in app_binaries:
        padded = pad_sector(data)
        off = sector * SECTOR_SIZE
        disk[off : off + len(padded)] = padded
        print(f"[embed_apps] {fname:12s}  sector={sector}  size={len(data)} bytes")

    # ---- Write FAT12 structures at LBA 500+ (matching BPB in boot.asm) ----
    fat_size = 3 * SECTOR_SIZE
    root_size = 4 * SECTOR_SIZE

    hello_size = len(app_binaries[0][2]) if len(app_binaries) > 0 else 0
    notes_content = b"Welcome to SQ Notes!\nCreate notes here.\nPress SAVE to save."
    readme_content = b"=== SQ-OS README ===\nWelcome to Antigravity OS!\nEnjoy the Task Manager,\nCalculator, Notes, and Settings.\nSQ-OS rocks!"

    e1 = make_dir_entry("HELLO", "ELF", 0x20, 2, hello_size)
    e2 = make_dir_entry("NOTES", "TXT", 0x20, 3, len(notes_content))
    e3 = make_dir_entry("README", "TXT", 0x20, 4, len(readme_content))

    root_sector = bytearray(root_size)
    root_sector[0:32] = e1
    root_sector[32:64] = e2
    root_sector[64:96] = e3

    fat1_off = RESERVED_SECTORS * SECTOR_SIZE
    fat2_off = (RESERVED_SECTORS + 3) * SECTOR_SIZE
    root_off = (RESERVED_SECTORS + 6) * SECTOR_SIZE
    data_off = (RESERVED_SECTORS + 10) * SECTOR_SIZE

    # Write FATs and Root directory
    disk[fat1_off : fat1_off + fat_size] = bytearray(fat_size)
    disk[fat2_off : fat2_off + fat_size] = bytearray(fat_size)
    disk[root_off : root_off + root_size] = root_sector

    # Write cluster contents (Cluster 2 = hello.elf, Cluster 3 = NOTES.TXT, Cluster 4 = README.TXT)
    if hello_size > 0:
        padded_hello = pad_sector(app_binaries[0][2])
        disk[data_off : data_off + len(padded_hello)] = padded_hello

    # NOTES.TXT sits at Cluster 3 (LBA 510 + size of HELLO.ELF sectors)
    # But wait, in a standard FAT12 root entry, cluster numbers are 1-based or 2-based.
    # The FAT12 driver expects files to be contiguous starting at their first cluster.
    # Cluster 3 starts 1 sector after Cluster 2:
    hello_sectors = (hello_size + SECTOR_SIZE - 1) // SECTOR_SIZE
    if hello_sectors == 0:
        hello_sectors = 1

    notes_off = data_off + hello_sectors * SECTOR_SIZE
    padded_notes = pad_sector(notes_content)
    disk[notes_off : notes_off + len(padded_notes)] = padded_notes

    # Update NOTES directory entry with its correct cluster index
    notes_cluster = 2 + hello_sectors
    e2 = make_dir_entry("NOTES", "TXT", 0x20, notes_cluster, len(notes_content))
    root_sector[32:64] = e2

    # README.TXT sits after NOTES
    readme_off = notes_off + len(padded_notes)
    padded_readme = pad_sector(readme_content)
    disk[readme_off : readme_off + len(padded_readme)] = padded_readme

    readme_cluster = notes_cluster + 1
    e3 = make_dir_entry("README", "TXT", 0x20, readme_cluster, len(readme_content))
    root_sector[64:96] = e3

    # Rewrite the updated Root sector
    disk[root_off : root_off + root_size] = root_sector

    # Write back to os.img
    with open(disk_path, 'wb') as f:
        f.write(bytes(disk))

    total_fs_sectors = 10 + hello_sectors + 1 + 1
    print(f"[embed_apps] Done. App directory LBA: {APP_DIR_SECTOR}. FAT12 LBA: {RESERVED_SECTORS}-{RESERVED_SECTORS + total_fs_sectors}.")


if __name__ == '__main__':
    apps = [
        ("HELLO.ELF", "hello.elf"),
        ("CRASH.APP", "crash.bin"),
    ]

    disk_path = 'os.img'
    if not os.path.exists(disk_path):
        print(f"[embed_apps] ERROR: {disk_path} not found")
        sys.exit(1)

    embed(disk_path, apps)
