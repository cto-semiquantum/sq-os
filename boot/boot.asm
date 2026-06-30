cpu 686
[org 0x7c00]
[bits 16]

jmp short start
nop

oem_name            db 'SQ-OS   '
bytes_per_sector    dw 512
sectors_per_cluster db 1
reserved_sectors    dw 500
num_fats            db 2
root_entry_count    dw 64
total_sectors_16    dw 2880
media_type          db 0xF0
fat_size_16         dw 3
sectors_per_track   dw 18
num_heads           dw 2
hidden_sectors      dd 0
total_sectors_32    dd 0

; Extended Boot Record
drive_number        db 0
reserved1           db 0
boot_sig            db 0x29
volume_id           dd 0x12345678
volume_label        db 'SQ-OS      '
fs_type             db 'FAT12   '

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; Save BIOS boot drive number (DL set by BIOS before jumping to MBR)
    mov [boot_drive], dl

    ; --- Try INT 13h Extended (LBA) read first (works on hard disk) ---
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jnc .disk_ok                ; carry clear = success

    ; --- Extended read failed: fall back to CHS INT 13h AH=02h ---
    mov ah, 0x02
    mov al, 120         ; Load 120 sectors (60 KB)
    mov ch, 0
    mov cl, 2           ; Start at sector 2 (CHS sector numbers are 1-based)
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, 0x7e00
    int 0x13
    jnc .disk_ok

    ; Disk read failed — print 'E' and halt
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
.hang:
    hlt
    jmp .hang

.disk_ok:

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
align 4
dap:
    db 0x10         ; packet size
    db 0            ; reserved
    dw 120          ; sectors to read
    dw 0x7e00       ; offset
    dw 0x0000       ; segment
    dd 1            ; LBA low
    dd 0            ; LBA high

boot_drive db 0

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
