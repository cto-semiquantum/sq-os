[org 0x7c00]

mov si, message

print:
    lodsb
    or al, al
    jz done

    mov ah, 0x0e
    int 0x10

    jmp print

done:
    jmp $

message db 'WELCOME TO SQ OS', 0

times 510-($-$$) db 0
dw 0xaa55