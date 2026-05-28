cpu 686
[org 0x7c00]
[bits 16]

; =============================================
; STAGE 1: Real Mode Loader
; =============================================
cli
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

; Load sectors 2-15 into 0x7e00
mov ah, 0x02
mov al, 14
mov ch, 0
mov cl, 2
mov dh, 0
mov dl, 0x80
mov bx, 0x7e00
int 0x13

; Remap 8259 PIC: IRQ0-7 -> INT 32-39
mov al, 0x11
out 0x20, al
out 0xA0, al
mov al, 0x20
out 0x21, al
mov al, 0x28
out 0xA1, al
mov al, 0x04
out 0x21, al
mov al, 0x02
out 0xA1, al
mov al, 0x01
out 0x21, al
out 0xA1, al
mov al, 0xFD   ; Unmask only IRQ1 (keyboard)
out 0x21, al
mov al, 0xFF
out 0xA1, al

; Load GDT
lgdt [gdt_descriptor]

; Enter Protected Mode
mov eax, cr0
or eax, 1
mov cr0, eax
jmp 0x08:0x7e00

; =============================================
; GDT
; =============================================
gdt_start:
    dq 0
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510-($-$$) db 0
dw 0xaa55

; =============================================
; STAGE 2: 32-BIT PROTECTED MODE SHELL
; Loaded at 0x7e00
; =============================================
[bits 32]

pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Build IDT at 0x2000
    mov edi, 0x2000
    mov ecx, 256
.idt_fill:
    mov eax, default_isr
    call make_gate
    loop .idt_fill

    mov edi, 0x2000 + 32*8
    mov eax, timer_isr
    call make_gate

    mov edi, 0x2000 + 33*8
    mov eax, keyboard_isr
    call make_gate

    mov word  [idt_limit], 256*8 - 1
    mov dword [idt_base],  0x2000
    lidt [idt_limit]

    ; Init variables
    mov dword [cur_row],    2
    mov dword [cur_col],    0
    mov dword [buf_len],    0

    ; Init input buffer at 0x3000
    mov edi, 0x3000
    mov ecx, 256
    xor al, al
    rep stosb

    ; Draw screen
    call clear_screen
    call draw_header
    call print_prompt

    sti

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
    mov  word [edi],   0x08
    add  edi, 2
    mov  byte [edi],   0x00
    inc  edi
    mov  byte [edi],   0x8E
    inc  edi
    shr  eax, 16
    mov  word [edi],   ax
    add  edi, 2
    pop  eax
    ret

; =============================================
; CLEAR SCREEN
; =============================================
clear_screen:
    mov edi, 0xB8000
    mov ecx, 2000
    mov ax, 0x0120   ; space, dark blue bg
    rep stosw
    ret

; =============================================
; DRAW HEADER (row 0)
; =============================================
draw_header:
    ; Row 0: Black bg, cyan text title bar
    mov edi, 0xB8000
    mov ecx, 80
    mov ax, 0xB020   ; space, cyan bg
    rep stosw

    ; Print title in center of header
    mov edi, 0xB8000 + 2*20
    mov esi, title_str
    mov ah, 0xB0   ; black on cyan
.ht: mov al, [esi]
    or al, al
    jz .htd
    stosw
    inc esi
    jmp .ht
.htd:

    ; Row 1: separator
    mov edi, 0xB8000 + 160
    mov ecx, 80
    mov ax, 0x0820   ; gray bg
    rep stosw
    ret

; =============================================
; PRINT PROMPT
; =============================================
print_prompt:
    ; Print "SQ> " at current cursor pos
    mov esi, prompt_str
    mov ah, 0x0B    ; cyan
.pp: mov al, [esi]
    or al, al
    jz .ppd
    call put_char_raw
    inc esi
    jmp .pp
.ppd:
    ret

; =============================================
; PUT CHAR AT current row/col, advance col
; =============================================
put_char_raw:
    ; al = char, ah = attribute
    push eax
    push edi
    mov edi, [cur_row]
    imul edi, edi, 160
    mov ecx, [cur_col]
    imul ecx, ecx, 2
    add edi, ecx
    add edi, 0xB8000
    stosw
    ; Advance col
    mov ecx, [cur_col]
    inc ecx
    cmp ecx, 80
    jl .pcr_col_ok
    xor ecx, ecx
    mov eax, [cur_row]
    inc eax
    cmp eax, 24
    jl .pcr_row_ok
    call scroll_screen
    mov eax, [cur_row]
.pcr_row_ok:
    mov [cur_row], eax
.pcr_col_ok:
    mov [cur_col], ecx
    pop edi
    pop eax
    ret

; =============================================
; NEW LINE — move to next row, col=0
; =============================================
new_line:
    mov dword [cur_col], 0
    mov eax, [cur_row]
    inc eax
    cmp eax, 24
    jl .nl_ok
    call scroll_screen
    mov eax, [cur_row]
.nl_ok:
    mov [cur_row], eax
    ret

; =============================================
; SCROLL SCREEN up by 1 row
; =============================================
scroll_screen:
    ; Copy rows 3..24 to rows 2..23 (protect header rows 0-1)
    mov esi, 0xB8000 + 160*3
    mov edi, 0xB8000 + 160*2
    mov ecx, 80*22   ; 22 rows of 80 words
    rep movsw
    ; Clear last row
    mov edi, 0xB8000 + 160*23
    mov ecx, 80
    mov ax, 0x0120
    rep stosw
    ; Keep cur_row at 23
    mov dword [cur_row], 23
    ret

; =============================================
; PRINT_STR: ESI=str, AH=attr, uses put_char_raw
; =============================================
print_str:
    push eax
.psr: mov al, [esi]
    or al, al
    jz .psrd
    call put_char_raw
    inc esi
    jmp .psr
.psrd:
    pop eax
    ret

; =============================================
; COMMAND DISPATCH
; =============================================
dispatch_command:
    ; Input buffer at 0x3000
    ; Compare against known commands
    mov esi, 0x3000

    mov edi, cmd_help
    call strcmp32
    jz do_help

    mov esi, 0x3000
    mov edi, cmd_clear
    call strcmp32
    jz do_clear

    mov esi, 0x3000
    mov edi, cmd_about
    call strcmp32
    jz do_about

    mov esi, 0x3000
    mov edi, cmd_version
    call strcmp32
    jz do_version

    mov esi, 0x3000
    mov edi, cmd_reboot
    call strcmp32
    jz do_reboot

    mov esi, 0x3000
    mov edi, cmd_gui
    call strcmp32
    jz do_gui

    ; Unknown command
    call new_line
    mov esi, err_str
    mov ah, 0x0C
    call print_str
    jmp dispatch_done

do_help:
    call new_line
    mov esi, help_str
    mov ah, 0x0A   ; green
    call print_str
    jmp dispatch_done

do_clear:
    call clear_screen
    call draw_header
    mov dword [cur_row], 2
    mov dword [cur_col], 0
    jmp dispatch_done

do_about:
    call new_line
    mov esi, about_str
    mov ah, 0x0B   ; cyan
    call print_str
    jmp dispatch_done

do_version:
    call new_line
    mov esi, version_str
    mov ah, 0x0F   ; white
    call print_str
    jmp dispatch_done

do_reboot:
    mov al, 0xFE
    out 0x64, al
    hlt

; =============================================
; GUI SUBSYSTEM (modular includes)
; =============================================
%include "gui/graphics.asm"
%include "gui/mouse.asm"
%include "gui/window.asm"
%include "gui/desktop.asm"
%include "gui/gui.asm"

dispatch_done:
    call new_line
    call print_prompt
    ret

; =============================================
; STRCMP32: ESI vs EDI, ZF=1 if equal
; =============================================
strcmp32:
    push eax
    push ebx
.lp:
    mov al, [esi]
    mov bl, [edi]
    cmp al, bl
    jne .fail
    test al, al
    jz .match
    inc esi
    inc edi
    jmp .lp
.match:
    pop ebx
    pop eax
    xor eax, eax
    ret
.fail:
    pop ebx
    pop eax
    or eax, 1
    ret

; =============================================
; DEFAULT / TIMER ISR
; =============================================
default_isr:
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    iret

timer_isr:
    mov al, 0x20
    out 0x20, al
    iret

; =============================================
; KEYBOARD ISR (IRQ1 = INT 33)
; =============================================
keyboard_isr:
    pushad
    in al, 0x60

    test al, 0x80
    jnz .kb_eoi

    cmp al, 0x39
    jg .kb_eoi

    xor ebx, ebx
    mov bl, al
    mov al, [scancode_map + ebx]
    or al, al
    jz .kb_eoi

    cmp al, 0x08
    je .backspace
    cmp al, 0x0A
    je .enter

    mov ecx, [buf_len]
    cmp ecx, 79
    jge .kb_eoi

    mov ebx, 0x3000
    add ebx, ecx
    mov [ebx], al
    inc dword [buf_len]

    mov ah, 0x0F
    call put_char_raw
    jmp .kb_eoi

.backspace:
    mov ecx, [buf_len]
    cmp ecx, 0
    je .kb_eoi
    dec dword [buf_len]
    mov ebx, 0x3000
    add ebx, ecx
    dec ebx
    mov byte [ebx], 0
    mov eax, [cur_col]
    cmp eax, 0
    je .kb_eoi
    dec dword [cur_col]
    mov eax, [cur_row]
    imul eax, eax, 160
    mov ecx, [cur_col]
    imul ecx, ecx, 2
    add eax, ecx
    add eax, 0xB8000
    mov word [eax], 0x0120
    jmp .kb_eoi

.enter:
    mov ecx, [buf_len]
    mov ebx, 0x3000
    add ebx, ecx
    mov byte [ebx], 0
    call new_line
    cmp dword [buf_len], 0
    je .empty_enter
    call dispatch_command
    jmp .clear_buf
.empty_enter:
    call print_prompt
.clear_buf:
    mov edi, 0x3000
    mov ecx, 80
    xor al, al
    rep stosb
    mov dword [buf_len], 0
    jmp .kb_eoi

.kb_eoi:
    mov al, 0x20
    out 0x20, al
    popad
    iret

; =============================================
; KERNEL CORE DATA
; =============================================
cur_row     dd 2
cur_col     dd 0
buf_len     dd 0
idt_limit   dw 0
idt_base    dd 0

title_str   db '  SQ-OS  |  32-BIT PROTECTED MODE SHELL  ', 0
prompt_str  db 'SQ> ', 0
err_str     db 'Unknown command. Type help.', 0
help_str    db 'Commands: help  clear  about  version  reboot  gui', 0
about_str   db 'SQ-OS | 32-bit Protected Mode | IRQ-driven Shell | by Harsh', 0
version_str db 'SQ-OS v2.0 | Kernel: pmode-32 | Arch: x86 PM | Build: 2026', 0

cmd_help    db 'help', 0
cmd_clear   db 'clear', 0
cmd_about   db 'about', 0
cmd_version db 'version', 0
cmd_gui     db 'gui', 0
cmd_reboot  db 'reboot', 0

; VGA Mode 13h register tables
crtc13h:
    db 0x00,0x5F, 0x01,0x4F, 0x02,0x50, 0x03,0x82, 0x04,0x54
    db 0x05,0x80, 0x06,0xBF, 0x07,0x1F, 0x08,0x00, 0x09,0x41
    db 0x0A,0x00, 0x0B,0x00, 0x0C,0x00, 0x0D,0x00, 0x10,0x9C
    db 0x11,0x8E, 0x12,0x8F, 0x13,0x28, 0x14,0x40, 0x15,0x96
    db 0x16,0xB9, 0x17,0xA3, 0x18,0xFF, 0x0E,0x00, 0x0F,0x00
gc13h:
    db 0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00
    db 0x05,0x40, 0x06,0x05, 0x07,0x0F, 0x08,0xFF
ac13h:
    db 0x00,0x00, 0x01,0x01, 0x02,0x02, 0x03,0x03, 0x04,0x04
    db 0x05,0x05, 0x06,0x06, 0x07,0x07, 0x08,0x08, 0x09,0x09
    db 0x0A,0x0A, 0x0B,0x0B, 0x0C,0x0C, 0x0D,0x0D, 0x0E,0x0E
    db 0x0F,0x0F, 0x10,0x41, 0x11,0x00, 0x12,0x0F, 0x13,0x00
    db 0x14,0x00
gctext:
    db 0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00
    db 0x05,0x10, 0x06,0x0E, 0x07,0x00, 0x08,0xFF

scancode_map:
    db 0, 27
    db '1','2','3','4','5','6','7','8','9','0'
    db '-','='
    db 8           ; Backspace
    db 9           ; Tab
    db 'q','w','e','r','t','y','u','i','o','p'
    db '[',']'
    db 10          ; Enter
    db 0           ; Ctrl
    db 'a','s','d','f','g','h','j','k','l'
    db ';',39,'`'
    db 0           ; LShift
    db 92          ; backslash
    db 'z','x','c','v','b','n','m'
    db ',','.','/'
    db 0           ; RShift
    db '*'
    db 0           ; Alt
    db ' '         ; Space

times 15360-($-$$) db 0
