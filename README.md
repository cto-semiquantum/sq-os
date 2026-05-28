# SQ-OS

A custom operating system built **from scratch in x86 Assembly** — bootloader, protected mode kernel, and a full graphical desktop environment. No C. No libraries. Pure bare-metal.

> **Status**: Actively developed — GUI milestone reached 🔥

---

## What This Is

SQ-OS boots directly on real hardware (or QEMU), switches from 16-bit real mode into **32-bit Protected Mode**, sets up its own IDT, handles PS/2 keyboard + mouse via direct hardware polling, and renders a graphical desktop using VGA Mode 13h (320×200, 256 colors).

This is **not** a tutorial project or a BIOS wrapper. Everything — memory layout, interrupt handling, font rendering, mouse packet parsing, double-buffering — is hand-coded in assembly.

---

## Current Features

### Bootloader (boot.asm)
- Stage 1: 512-byte MBR bootloader
- Loads 30 sectors from disk into memory at `0x7E00`
- Jumps to Stage 2 kernel

### 32-bit Protected Mode Kernel (pmode.asm)
- GDT setup with flat 4GB code + data segments
- IDT with 256 gates — timer ISR, keyboard ISR, default handler
- PIC remapping (IRQ0-7 → INT 32-39)
- IRQ-driven keyboard input with scancode → ASCII translation
- Interactive shell with commands: `help`, `about`, `version`, `clear`, `reboot`, `gui`
- Text-mode VGA rendering (80×25, color attributes)
- Stack at `0x90000` (safe conventional memory)

### GUI Desktop (type `gui` in shell)
- **VGA Mode 13h** — 320×200, 256 colors, direct framebuffer at `0xA0000`
- **Double buffering** — renders to backbuffer at `0x50000`, blits to screen atomically (no flicker/trails)
- **Stable render loop**: clear → desktop → taskbar → icons → windows → cursor (always last)
- **PS/2 mouse** — direct hardware polling via port `0x64`/`0x60`, 3-byte packet parsing, sign-extension, bounds clamping
- **Cursor** — 5×5 white square with black center dot, always rendered on top
- **Clickable desktop icons** — FILES icon with hitbox detection
- **Window system** — Win95-style 3D bevelled windows with title bar + close button
- **SQ Files app** — opens on click, shows file list with icons, status bar
- **ESC** — returns to shell

### GUI Subsystem Architecture
```
pmode.asm          ← bootloader + PM entry + shell + ISRs
gui/
├── graphics.asm   ← draw_rect_pm, draw_window_pm, draw_char, draw_text, 8x8 font
├── mouse.asm      ← PS/2 mouse state variables
├── window.asm     ← draw_cursor_pm, draw_files_window, window data
└── desktop.asm    ← redraw_desktop_pm (full frame pipeline)
```

---

## Build

```bash
# Compile the protected mode kernel (self-contained bootloader + kernel)
nasm -f bin pmode.asm -o pmode.bin
```

---

## Run

```bash
qemu-system-x86_64 -drive format=raw,file=pmode.bin -m 4M
```

Then at the shell prompt:
```
SQ> gui
```

---

## Project Structure

```
SQ-OS/
├── pmode.asm          ← Main: real-mode loader + 32-bit PM kernel + shell
├── boot.asm           ← Simple 16-bit bootloader (legacy)
├── kernel.asm         ← 16-bit real-mode kernel (legacy)
├── shell.asm          ← 16-bit shell (legacy)
├── graphics.asm       ← 16-bit graphics (legacy)
├── gui/
│   ├── graphics.asm   ← PM GUI: primitives + font
│   ├── mouse.asm      ← PM GUI: mouse state
│   ├── window.asm     ← PM GUI: windows + cursor
│   └── desktop.asm    ← PM GUI: frame pipeline
├── Makefile
└── build.bat
```

---

## Tech Stack

| Layer | Tech |
|-------|------|
| Language | x86 Assembly (NASM) |
| Mode | 32-bit Protected Mode (flat) |
| Graphics | VGA Mode 13h (320×200×256) |
| Input | PS/2 keyboard + mouse (direct hardware) |
| Emulator | QEMU |
| No dependencies | No C, no libc, no BIOS in GUI mode |

---

## Milestones Reached

- [x] Custom bootloader
- [x] Protected mode switch (GDT, IDT, PIC)
- [x] IRQ-driven keyboard shell
- [x] VGA Mode 13h activation (pure register programming)
- [x] PS/2 mouse packet parsing
- [x] Double-buffered GUI render loop
- [x] Clickable desktop icons
- [x] Window system with open/close
- [x] Modular GUI subsystem (`gui/` directory)

## Roadmap

- [ ] Arrow cursor sprite
- [ ] Terminal window inside GUI
- [ ] Window manager (z-order, focus, multiple windows)
- [ ] Real FAT12 filesystem (read actual files)
- [ ] More desktop apps

---

## Author

**Harsh Jha** — SemiQuantum  

Built from scratch. No shortcuts.
