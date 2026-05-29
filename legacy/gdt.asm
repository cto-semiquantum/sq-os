; =============================================
; GLOBAL DESCRIPTOR TABLE (GDT)
; =============================================

gdt_start:
    dq 0x0                ; Null descriptor

gdt_code:
    dw 0xFFFF             ; Limit (bits 0-15)
    dw 0x0000             ; Base (bits 0-15)
    db 0x00               ; Base (bits 16-23)
    db 10011010b          ; Access byte (exec/read)
    db 11001111b          ; Flags & Limit (bits 16-19)
    db 0x00               ; Base (bits 24-31)

gdt_data:
    dw 0xFFFF             ; Limit (bits 0-15)
    dw 0x0000             ; Base (bits 0-15)
    db 0x00               ; Base (bits 16-23)
    db 10010010b          ; Access byte (read/write)
    db 11001111b          ; Flags & Limit (bits 16-19)
    db 0x00               ; Base (bits 24-31)

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; Size
    dd gdt_start                 ; Address

; Segment selectors
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
