# SQ-OS

A custom operating system built using a hybrid **C and x86 Assembly** architecture — featuring a custom 32-bit Protected Mode kernel, a graphical windowing system, a program loader, a heap memory manager, and basic FAT12 filesystem support.

> **Status**: Actively developed — Milestone 3 reached! program loader, heap memory manager, and filesystem foundation are fully operational. 🔥

---

## What This Is

SQ-OS boots directly on virtualized or real hardware, transitions from 16-bit real mode into **32-bit Protected Mode**, initializes GDT/IDT descriptor tables, handles PS/2 keyboard + mouse interrupts, and implements a graphical windowing desktop using VGA Mode 13h (320×200, 256 colors). 

With the latest milestone, the OS has transitioned from a pure Assembly codebase to a modular **C + ASM hybrid** layout, complete with an external program loader, dynamic memory allocation, and disk access capability.

---

## Key Features

### 32-bit Protected Mode Kernel
- **GDT Setup**: Flat 4GB code and data descriptor segments.
- **IDT and Interrupts**: Custom interrupt handler gates for keyboard, mouse, and RTC timer.
- **ATA PIO Disk Driver**: Read sectors directly from hard disk using port I/O.
- **PIC Remapping**: IRQ0–7 mapped to INT 32–39, IRQ8–15 mapped to INT 40–47.

### Memory Management
- **Kernel Heap**: Located at the 1MB physical boundary (`0x100000`) with a 256KB arena.
- **Bump Allocator**: Implements `kmalloc(size)` and `kfree(ptr)` to handle dynamic buffer allocation for windows, apps, and program loading.
- **Memory Diagnostics**: Use the `mem` terminal command to view heap statistics (used, free, and block counts).

### Program Loader & App Store
- **Raw Sector App Store**: Sectors starting at `80` store application files on the raw disk image.
- **Position-Independent Binaries**: App binaries (such as `hello.asm`) use a call/pop PIC base recovery technique so they execute cleanly from any heap allocation address.
- **Dynamic Program Execution**: Run external applications using the `run <app.app>` command. It allocates heap memory, reads program sectors, executes via function pointers, and frees memory upon exit.

### FAT12 Filesystem Foundation
- **BPB Structure Support**: Real FAT12 volume definitions.
- **Directory Reader**: Reads sector-based listings and lists files. Use the `files` command to query the disk.

### Graphical Desktop Environment
- **VGA Mode 13h**: 320×200, 256 colors with double-buffering at `0x50000` to eliminate rendering flicker.
- **Retro Gradient Wallpaper**: A custom procedurally generated wallpaper renders as the desktop background.
- **Window Manager**: Handles window hierarchy (Z-order), mouse click focus changes, title bars, and draggable windows.
- **Advanced Terminal Application**:
  - 8-line output history ring buffer.
  - Command recall history using **Up/Down arrows**.
  - Boundary-clipped text rendering (`draw_text_clipped`) to prevent text from bleeding outside the window frames.
  - Interactive commands: `help`, `about`, `version`, `mem`, `files`, `apps`, `run <app>`, `reboot`.

---

## Project Directory Structure

```
SQ-OS/
├── apps/               ← External applications (e.g., hello.asm)
├── assets/             ← Wallpaper generation script and static data
├── boot/               ← 16-bit Stage 1 bootloader (boot.asm)
├── drivers/            ← Hardware drivers (mouse.c, mouse.h)
├── fs/                 ← Filesystem foundation (fat12.c, fat12.h)
├── gui/                ← Reference legacy Assembly GUI files
├── include/            ← Kernel C API headers
├── kernel/             ← Kernel entry point (entry.asm) and C subsystems
├── legacy/             ← Reorganized pure Assembly codebase files
├── build.bat           ← Integrated build pipeline script
├── linker.ld           ← Linker script for compiling 32-bit ELF binaries
└── README.md           ← Project documentation
```

---

## Build and Run

### Prerequisites
- [NASM](https://www.nasm.us/)
- i686-elf-gcc Toolchain (with `i686-elf-gcc` and `i686-elf-ld` in system path)
- Python 3
- QEMU Emulator (`qemu-system-i386`)

### Build Pipeline
Run the build script to compile the assembly, build all C source files, link the hybrid kernel, pad the raw image, and embed the application store:

```cmd
.\build.bat
```

### Emulate
Launch the raw image in QEMU:

```cmd
qemu-system-i386 -drive format=raw,file=os.img -m 32M
```

*Inside the emulator, you can exit the GUI and return to the text shell at any time by pressing **ESC**.*

---

## Authors & Acknowledgement

- **Harsh Jha** — SemiQuantum

Built from the hardware up. No shortcuts.
