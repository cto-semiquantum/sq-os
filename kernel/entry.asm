[bits 32]

section .entry

global _start
global default_isr
global timer_isr
global keyboard_isr
global syscall_isr
global setjmp
global longjmp

extern kernel_main
extern keyboard_handler
extern syscall_handler

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

    ; Install Syscall ISR at INT 128 (0x80)
    mov edi, 0x2000 + 128*8
    mov eax, syscall_isr
    call make_syscall_gate

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
; MAKE IDT GATE at EDI for handler EAX (DPL=0)
; =============================================
make_gate:
    push eax
    mov  word [edi],   ax
    add  edi, 2
    mov  word [edi],   0x08     ; Code segment selector (0x08)
    add  edi, 2
    mov  byte [edi],   0x00
    inc  edi
    mov  byte [edi],   0x8E     ; 32-bit Interrupt Gate, DPL=0
    inc  edi
    shr  eax, 16
    mov  word [edi],   ax
    add  edi, 2
    pop  eax
    ret

; =============================================
; MAKE SYSCALL GATE at EDI for handler EAX (DPL=3)
; =============================================
make_syscall_gate:
    push eax
    mov  word [edi],   ax
    add  edi, 2
    mov  word [edi],   0x08     ; Code segment selector (0x08)
    add  edi, 2
    mov  byte [edi],   0x00
    inc  edi
    mov  byte [edi],   0xEE     ; 32-bit Interrupt Gate, DPL=3
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

syscall_isr:
    pushad              ; Save general-purpose registers

    ; Push arguments for syscall_handler(num, arg1, arg2, arg3, arg4, arg5)
    push edi            ; arg5
    push esi            ; arg4
    push edx            ; arg3
    push ecx            ; arg2
    push ebx            ; arg1
    push eax            ; num
    
    call syscall_handler
    add esp, 24         ; Clean up parameters from stack

    ; Save return value in the saved EAX slot of pushad structure
    ; EAX is at offset 28 on the stack after pushad
    mov [esp + 28], eax

    popad               ; Restore all registers (EAX gets the return value)
    iret

; =============================================
; SETJMP & LONGJMP (Cooperative context switch)
; =============================================
setjmp:
    mov edx, [esp + 4]  ; edx = jmp_buf
    mov [edx], ebp
    mov [edx + 4], ebx
    mov [edx + 8], edi
    mov [edx + 12], esi
    lea ecx, [esp + 4]  ; original ESP before setjmp call
    mov [edx + 16], ecx
    mov ecx, [esp]      ; return address (EIP)
    mov [edx + 20], ecx
    xor eax, eax        ; return 0
    ret

longjmp:
    mov edx, [esp + 4]  ; edx = jmp_buf
    mov eax, [esp + 8]  ; eax = return value
    test eax, eax
    jnz .not_zero
    mov eax, 1          ; longjmp cannot return 0
.not_zero:
    mov ebp, [edx]
    mov ebx, [edx + 4]
    mov edi, [edx + 8]
    mov esi, [edx + 12]
    mov esp, [edx + 16] ; restore ESP
    mov ecx, [edx + 20] ; restore EIP
    jmp ecx             ; jump to saved EIP

; =============================================
; IDT Descriptor Data
; =============================================
idt_limit dw 0
idt_base  dd 0
