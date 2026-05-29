[bits 32]
[org 0]

; ============================================================
; hello.app — SQ-OS System Call Test Application
; ============================================================

_start:
    ; ---- PIC base recovery ----
    call .here
.here:
    pop ebx                             ; ebx = runtime address of .here

    ; Calculate address of msg
    lea ecx, [ebx + (msg - .here)]

    ; Call sys_print(msg)
    mov eax, 1                          ; EAX = 1 (SYS_PRINT)
    mov ebx, ecx                        ; EBX = msg address
    int 0x80                            ; Call interrupt 0x80

    ; Call sys_exit(0)
    mov eax, 5                          ; EAX = 5 (SYS_EXIT)
    mov ebx, 0                          ; EBX = 0 (exit code)
    int 0x80                            ; Call interrupt 0x80

    ; Fallback halt
.halt:
    hlt
    jmp .halt

msg: db "Hello from SQ System Call!", 0
