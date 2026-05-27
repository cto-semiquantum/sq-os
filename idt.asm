; =============================================
; INTERRUPT DESCRIPTOR TABLE (IDT)
; =============================================

; IDT entry structure (8 bytes in 32-bit mode)
; struct idt_entry {
;     uint16_t base_lo;
;     uint16_t sel;
;     uint8_t  always0;
;     uint8_t  flags;
;     uint16_t base_hi;
; };

idt_descriptor:
    dw 256 * 8 - 1      ; Limit (256 entries)
    dd 0                ; Base address of IDT (set at runtime)
