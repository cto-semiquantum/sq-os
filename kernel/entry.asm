[bits 32]

section .entry

global _start
global default_isr
global timer_isr
global keyboard_isr

extern kernel_main
extern keyboard_handler

_start:
    ; Set segment registers to data selector (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000    ; Set stack pointer

    ; Build IDT at 0x2000
    mov edi, 0x2000
    mov ecx, 256
.idt_fill:
    mov eax, default_isr
    call make_gate
    loop .idt_fill

    ; Install Timer ISR at INT 32 (IRQ0)
    mov edi, 0x2000 + 32*8
    mov eax, timer_isr
    call make_gate

    ; Install Keyboard ISR at INT 33 (IRQ1)
    mov edi, 0x2000 + 33*8
    mov eax, keyboard_isr
    call make_gate

    ; Load IDT
    mov word  [idt_limit], 256*8 - 1
    mov dword [idt_base],  0x2000
    lidt [idt_limit]

    ; Call the C kernel entry point
    call kernel_main

.idle:
    hlt
    jmp .idle

; =============================================
; MAKE IDT GATE at EDI for handler EAX
; =============================================
make_gate:
    push eax
    mov  word [edi],   ax
    add  edi, 2
    mov  word [edi],   0x08     ; Code segment selector (0x08)
    add  edi, 2
    mov  byte [edi],   0x00
    inc  edi
    mov  byte [edi],   0x8E     ; 32-bit Interrupt Gate
    inc  edi
    shr  eax, 16
    mov  word [edi],   ax
    add  edi, 2
    pop  eax
    ret

; =============================================
; INTERRUPT SERVICE ROUTINES
; =============================================
default_isr:
    push eax
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    pop eax
    iret

timer_isr:
    push eax
    mov al, 0x20
    out 0x20, al
    pop eax
    iret

keyboard_isr:
    pushad
    xor eax, eax
    in al, 0x60
    push eax            ; Pass scancode to C handler
    call keyboard_handler
    add esp, 4
    mov al, 0x20
    out 0x20, al
    popad
    iret

; =============================================
; IDT Descriptor Data
; =============================================
idt_limit dw 0
idt_base  dd 0
