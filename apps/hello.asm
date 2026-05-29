[bits 32]
[org 0]

; ============================================================
; hello.app — SQ-OS Test Application
; ============================================================
; ABI (32-bit cdecl):
;   void entry(char *out_buf, uint32_t buf_size)
;   [ebp+8]  = out_buf  (write null-terminated result here)
;   [ebp+12] = buf_size (max bytes available)
;
; This binary is position-independent (PIC).
; The call/pop trick recovers the runtime base address so
; that the embedded string can be referenced by absolute
; address regardless of where the loader puts us in memory.
;
; Assembled as a flat binary (no ELF/PE headers).
; Loader embeds it on disk with a 4-byte size prefix.
; ============================================================

_start:
    push ebp
    mov  ebp, esp

    ; ---- PIC base recovery ----
    ; After "call .here", the stack top = runtime address of .here.
    call .here
.here:
    pop  ebx                            ; ebx = runtime addr of .here

    ; esi = runtime address of hello_msg
    lea  esi, [ebx + (hello_msg - .here)]

    ; edi = out_buf (first argument)
    mov  edi, [ebp + 8]

    ; ---- Copy string (including null terminator) ----
.copy:
    lodsb                               ; al = *esi++
    stosb                               ; *edi++ = al
    test al, al
    jnz  .copy

    pop  ebp
    ret

; ---- Embedded string ----
hello_msg: db "Hello from SQ Program Loader", 0
