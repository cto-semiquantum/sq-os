cpu 686
[org 0x7c00]
[bits 16]

; =============================================
; STAGE 1: Real Mode Bootloader
; =============================================
cli
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; Load Stage 2 Kernel (sectors 2 to 51) into 0x7e00 (approx 25 KB)
mov ah, 0x02
mov al, 50          ; Load 50 sectors
mov ch, 0
mov cl, 2           ; Start at sector 2
mov dh, 0
mov dl, 0x80        ; First hard drive (QEMU default drive)
mov bx, 0x7e00
int 0x13

; Remap 8259 PIC: IRQ0-7 -> INT 32-39
mov al, 0x11
out 0x20, al
out 0xA0, al
mov al, 0x20
out 0x21, al
mov al, 0x28
out 0xA1, al
mov al, 0x04
out 0x21, al
mov al, 0x02
out 0xA1, al
mov al, 0x01
out 0x21, al
out 0xA1, al

; Unmask IRQ1 (keyboard) and IRQ12 (mouse)
; Keyboard is IRQ1 (bit 1 of master PIC, mask=0xFD)
; Mouse is IRQ12 (bit 4 of slave PIC, mask=0xEF)
mov al, 0xFD
out 0x21, al
mov al, 0xEF
out 0xA1, al

; Load GDT
lgdt [gdt_descriptor]

; Enter Protected Mode
mov eax, cr0
or eax, 1
mov cr0, eax

; Far jump to clear CPU pipeline and enter 32-bit PM at 0x7e00
jmp 0x08:0x7e00

; =============================================
; Global Descriptor Table (GDT)
; =============================================
gdt_start:
    dq 0            ; Null descriptor
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF   ; Code segment (0x08)
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF   ; Data segment (0x10)
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510-($-$$) db 0
dw 0xaa55
