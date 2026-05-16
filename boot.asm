[org 0x7c00]

; =================
; STAGE 1: BOOTLOADER (Sector 1)
; =================
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; Load kernel from disk
mov ah, 0x02
mov al, 20        ; Load 20 sectors (10KB) to be absolutely safe
mov ch, 0
mov cl, 2
mov dh, 0
mov bx, 0x7e00
int 0x13

jmp 0x7e00

times 510-($-$$) db 0
dw 0xaa55
