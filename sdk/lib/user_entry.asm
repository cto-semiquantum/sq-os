[bits 32]

section .text
global _start
extern main

_start:
    ; Call user application entry point
    call main

    ; Exit system call (SYS_EXIT = 5) if main returns
    mov eax, 5
    mov ebx, 0          ; exit code = 0
    int 0x80

    ; Spin if we ever return
.halt:
    hlt
    jmp .halt
