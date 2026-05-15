# SQ OS

A custom operating system project built from scratch using x86 Assembly.

Currently running as a bootable binary inside QEMU with:
- Custom bootloader
- VGA text mode rendering
- Interactive shell input
- Keyboard handling
- Real-mode execution
- Retro terminal style UI

## Current Features

- Bootable `.bin` image
- Custom terminal interface
- User keyboard input
- Dynamic shell prompt
- VGA memory text rendering
- QEMU support

## Tech Stack

- x86 Assembly (NASM)
- QEMU Emulator
- BIOS Interrupts
- VGA Text Mode

## Build

```bash
nasm -f bin boot.asm -o boot.bin
```

## Run

```bash
qemu-system-x86_64 -drive format=raw,file=boot.bin
```

## Project Structure

```bash
SQ-OS/
├── boot.asm
├── boot.bin
├── Makefile
├── branding/
├── configs/
├── docs/
├── iso/
├── screenshots/
├── scripts/
└── ui/
```

## Screenshot

SQ OS currently boots into a minimal shell interface running directly on bare metal architecture through QEMU.

## Roadmap

- Command parser
- Backspace support
- Kernel separation
- Protected mode
- Memory management
- File system
- GUI layer

## Author

Harsh Jha  
SemiQuantum
