[bits 32]

section .entry

global _start
global default_isr
global timer_isr
global keyboard_isr
global syscall_isr
global page_fault_isr
global gdt_flush
global tss_flush
global setjmp
global longjmp

extern kernel_main
extern keyboard_handler
extern syscall_handler
extern timer_handler
extern scheduler_switch
extern page_fault_handler
extern update_tss_esp0

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

    ; Install Page Fault ISR at INT 14 (#PF)
    mov edi, 0x2000 + 14*8
    mov eax, page_fault_isr
    call make_gate

    ; Install Timer ISR at INT 32 (IRQ0)
    mov edi, 0x2000 + 32*8
    mov eax, timer_isr
    call make_gate

    ; Install Keyboard ISR at INT 33 (IRQ1)
    mov edi, 0x2000 + 33*8
    mov eax, keyboard_isr
    call make_gate

    ; Install Syscall ISR at INT 128 (0x80) with DPL=3 so Ring 3 can call it
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
    pushad
    call timer_handler

    push esp
    call scheduler_switch
    add  esp, 4
    ; eax = new process ESP — save it while we update TSS
    push eax
    call update_tss_esp0   ; update TSS.esp0 = current_process->kernel_stack_top
    pop  eax               ; restore new process ESP

    mov  esp, eax          ; switch to new process kernel stack

    mov al, 0x20
    out 0x20, al
    popad
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

    ; Reload kernel data segments in case caller was Ring 3 (DS = 0x23)
    push dword 0x10
    pop  ds
    push dword 0x10
    pop  es

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

    ; Restore user data segment before returning to Ring 3
    ; (If Ring 0, 0x10 is correct; if Ring 3, CPU doesn't enforce DS on iret
    ;  but we set it explicitly for consistency)
    push dword 0x23
    pop  ds
    push dword 0x23
    pop  es

    popad               ; Restore all registers (EAX gets the return value)
    iret

; =============================================
; PAGE FAULT ISR (INT 14 — #PF)
;
; Stack layout after CPU entry (Ring 3 fault):
;   [ESP+0]  = Error Code   (CPU pushed)
;   [ESP+4]  = EIP          (faulting instruction)
;   [ESP+8]  = CS
;   [ESP+12] = EFLAGS
;   [ESP+16] = User ESP3    (Ring 3 only)
;   [ESP+20] = User SS3     (Ring 3 only)
; =============================================
page_fault_isr:
    pushad              ; Save all GP registers (32 bytes)

    ; After pushad:
    ;   [esp+0]  = EDI ... [esp+28] = EAX
    ;   [esp+32] = Error Code
    ;   [esp+36] = Faulting EIP

    mov  eax, cr2                   ; Fault address
    push eax                        ; arg2: fault_addr   (esp was -4)
    push dword [esp + 36]           ; arg1: error_code   (esp was -8)
                                    ; (pushad=32, push fault=4, so error_code at 32+4+4=40? No:
                                    ;  after pushad: error at [esp+32]
                                    ;  after push eax: error at [esp+36] ← correct)

    call page_fault_handler         ; returns 0 = ring3 handled, else panic
    add  esp, 8

    test eax, eax
    jnz  .pf_panic                  ; non-zero = kernel fault → halt

    ; Ring 3 process was terminated — forced reschedule (same as timer_isr)
    push esp
    call scheduler_switch
    add  esp, 4
    push eax
    call update_tss_esp0
    pop  eax
    mov  esp, eax                   ; switch to next ready process stack

    ; Next process stack: pushad frame + iret frame (NO error code)
    popad
    iret                            ; resume next process

.pf_panic:
    cli
.pf_halt:
    hlt
    jmp .pf_halt

; =============================================
; GDT FLUSH — reload GDT and segment registers
; void gdt_flush(uint32_t gdt_ptr_addr)
; =============================================
gdt_flush:
    mov eax, [esp + 4]  ; pointer to GDTPtr struct
    lgdt [eax]

    ; Far-return trick to reload CS with new GDT (selector 0x08)
    push dword 0x08
    push dword .gf_cs
    retf
.gf_cs:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

; =============================================
; TSS FLUSH — load Task Register with 0x28
; =============================================
tss_flush:
    mov ax, 0x28        ; TSS selector (GDT entry 5)
    ltr ax
    ret

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
