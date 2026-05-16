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
    mov esp, 0x9000

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

do_gui:
    cli
    ; === SET VGA MODE 13h via direct register writes ===
    ; Helper: vga_write - al=index, ah=value, dx=base port
    ; Misc Output
    mov dx, 0x3C2
    mov al, 0x63
    out dx, al

    ; Sequencer
    mov dx, 0x3C4
    mov al, 0x00 ; index
    out dx, al
    inc dx
    mov al, 0x03 ; reset
    out dx, al
    dec dx
    mov al, 0x01
    out dx, al
    inc dx
    mov al, 0x01
    out dx, al
    dec dx
    mov al, 0x02
    out dx, al
    inc dx
    mov al, 0x0F
    out dx, al
    dec dx
    mov al, 0x03
    out dx, al
    inc dx
    mov al, 0x00
    out dx, al
    dec dx
    mov al, 0x04
    out dx, al
    inc dx
    mov al, 0x0E
    out dx, al

    ; Unlock CRTC
    mov dx, 0x3D4
    mov al, 0x11
    out dx, al
    inc dx
    mov al, 0x8E
    out dx, al
    dec dx

    ; CRTC registers
    mov esi, crtc13h
    mov ecx, 25
.crtc_loop:
    mov al, [esi]
    out dx, al
    inc dx
    inc esi
    mov al, [esi]
    out dx, al
    dec dx
    inc esi
    loop .crtc_loop

    ; Graphics Controller
    mov dx, 0x3CE
    mov esi, gc13h
    mov ecx, 9
.gc_loop:
    mov al, [esi]
    out dx, al
    inc dx
    inc esi
    mov al, [esi]
    out dx, al
    dec dx
    inc esi
    loop .gc_loop

    ; Attribute Controller — reset flip-flop first
    mov dx, 0x3DA
    in al, dx
    mov dx, 0x3C0
    mov esi, ac13h
    mov ecx, 21
.ac_loop:
    mov al, [esi]
    out dx, al
    inc esi
    mov al, [esi]
    out dx, al
    inc esi
    loop .ac_loop
    mov al, 0x20
    out dx, al

    sti

    ; === DRAW DESKTOP ===
    ; Black background
    mov edi, 0xA0000
    mov ecx, 64000
    xor al, al
    rep stosb

    ; Dark blue desktop fill
    mov edi, 0xA0000
    mov ecx, 60800
    mov al, 1
    rep stosb

    ; Cyan top bar (row 0-9)
    mov edi, 0xA0000
    mov ecx, 3200
    mov al, 3
    rep stosb

    ; Gray taskbar bottom
    mov edi, 0xA0000 + 60800
    mov ecx, 3200
    mov al, 8
    rep stosb

    ; White start button area
    mov edi, 0xA0000 + 60820
    mov ecx, 400
    mov al, 15
    rep stosb

    ; Yellow icon 1
    mov edi, 0xA0000 + 6500
    mov ecx, 400
    mov al, 14
    rep stosb

    ; Green icon 2
    mov edi, 0xA0000 + 9500
    mov ecx, 400
    mov al, 10
    rep stosb

    ; Red icon 3
    mov edi, 0xA0000 + 12500
    mov ecx, 400
    mov al, 12
    rep stosb

    ; === RENDER TEXT ON DESKTOP ===
    mov esi, gui_title
    mov ebx, 96
    mov ecx, 1
    mov dl, 15
    call draw_text

    mov esi, lbl_files
    mov ebx, 18
    mov ecx, 38
    mov dl, 15
    call draw_text

    mov esi, lbl_term
    mov ebx, 0
    mov ecx, 53
    mov dl, 11
    call draw_text

    mov esi, lbl_set
    mov ebx, 2
    mov ecx, 68
    mov dl, 14
    call draw_text

    mov esi, lbl_esc
    mov ebx, 80
    mov ecx, 191
    mov dl, 7
    call draw_text

    ; Poll for ESC key (scancode 0x81 = ESC release)
.gui_poll:
    in al, 0x64
    test al, 0x01  ; output buffer full?
    jz .gui_poll
    in al, 0x60
    cmp al, 0x01   ; ESC press
    jne .gui_poll

    ; === RESTORE TEXT MODE 3 ===
    cli
    mov al, 0x67
    mov dx, 0x3C2
    out dx, al

    ; Sequencer
    mov dx, 0x3C4
    mov al, 0x00
    out dx, al
    inc dx
    mov al, 0x03
    out dx, al
    dec dx
    mov al, 0x01
    out dx, al
    inc dx
    mov al, 0x00
    out dx, al
    dec dx
    mov al, 0x02
    out dx, al
    inc dx
    mov al, 0x03
    out dx, al
    dec dx
    mov al, 0x03
    out dx, al
    inc dx
    mov al, 0x00
    out dx, al
    dec dx
    mov al, 0x04
    out dx, al
    inc dx
    mov al, 0x02
    out dx, al

    ; GC text mode
    mov dx, 0x3CE
    mov esi, gctext
    mov ecx, 9
.gc_text:
    mov al, [esi]
    out dx, al
    inc dx
    inc esi
    mov al, [esi]
    out dx, al
    dec dx
    inc esi
    loop .gc_text

    sti
    call clear_screen
    call draw_header
    mov dword [cur_row], 2
    mov dword [cur_col], 0
    jmp dispatch_done

; ==========================================
; BITMAP FONT RENDERER (8x8 pixels/char)
; ==========================================
; draw_char: EBX=x, ECX=y, AL=char, AH=color
draw_char:
    mov [dc_char],  al
    mov [dc_color], ah
    mov [dc_basex], ebx
    mov [dc_basey], ecx
    pushad                      ; save ALL registers (fixes esi corruption in draw_text)
    movzx eax, byte [dc_char]
    sub eax, 32
    jl .dc_exit
    shl eax, 3
    add eax, font8x8
    mov [dc_glyph], eax     ; pointer to 8-byte glyph

    mov dword [dc_row], 0
.dc_row:
    cmp dword [dc_row], 8
    jge .dc_exit

    mov esi, [dc_glyph]
    mov ecx, [dc_row]
    movzx eax, byte [esi + ecx]  ; glyph row byte
    mov [dc_bits], al

    mov dword [dc_col], 0
.dc_col:
    cmp dword [dc_col], 8
    jge .dc_next_row

    mov al, [dc_bits]
    test al, 0x80
    jz .dc_skip

    ; pixel_addr = (basey+row)*320 + basex+col + 0xA0000
    mov eax, [dc_basey]
    add eax, [dc_row]
    imul eax, eax, 320
    add eax, [dc_basex]
    add eax, [dc_col]
    add eax, 0xA0000
    movzx ebx, byte [dc_color]
    mov [eax], bl

.dc_skip:
    mov al, [dc_bits]
    shl al, 1
    mov [dc_bits], al
    inc dword [dc_col]
    jmp .dc_col

.dc_next_row:
    inc dword [dc_row]
    jmp .dc_row

.dc_exit:
    popad                       ; restore all registers including esi
    ret

; draw_text: ESI=str, EBX=x, ECX=y, DL=color
draw_text:
    pushad
.dt_loop:
    mov al, [esi]
    or al, al
    jz .dt_done
    mov ah, dl
    call draw_char
    add ebx, 8
    inc esi
    jmp .dt_loop
.dt_done:
    popad
    ret

dispatch_done:
    call new_line
    call print_prompt
    ret

; =============================================
; STRCMP32: ESI vs EDI, ZF=1 if match
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
    xor eax, eax    ; ZF=1
    ret
.fail:
    pop ebx
    pop eax
    or eax, 1       ; ZF=0
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

    cmp al, 0x08    ; Backspace
    je .backspace

    cmp al, 0x0A    ; Enter
    je .enter

    ; Add char to buffer if room
    mov ecx, [buf_len]
    cmp ecx, 79
    jge .kb_eoi

    ; Store in buffer
    mov ebx, 0x3000
    add ebx, ecx
    mov [ebx], al
    inc dword [buf_len]

    ; Echo to screen
    mov ah, 0x0F
    call put_char_raw
    jmp .kb_eoi

.backspace:
    mov ecx, [buf_len]
    cmp ecx, 0
    je .kb_eoi
    dec dword [buf_len]
    ; Remove from buffer
    mov ebx, 0x3000
    add ebx, ecx
    dec ebx
    mov byte [ebx], 0
    ; Erase from screen
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
    ; Null-terminate buffer
    mov ecx, [buf_len]
    mov ebx, 0x3000
    add ebx, ecx
    mov byte [ebx], 0
    call new_line
    ; Dispatch if non-empty
    cmp dword [buf_len], 0
    je .empty_enter
    call dispatch_command
    jmp .clear_buf
.empty_enter:
    call print_prompt
.clear_buf:
    ; Clear buffer
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
; DATA
; =============================================
cur_row   dd 2
cur_col   dd 0
buf_len   dd 0
idt_limit dw 0
idt_base  dd 0

title_str   db '  SQ-OS  |  32-BIT PROTECTED MODE SHELL  ', 0
prompt_str  db 'SQ> ', 0
err_str     db 'Unknown command. Type help.', 0
help_str    db 'Commands: help  clear  about  version  reboot', 0
about_str   db 'SQ-OS | 32-bit Protected Mode | IRQ-driven Shell | by Harsh', 0
version_str db 'SQ-OS v2.0 | Kernel: pmode-32 | Arch: x86 PM | Build: 2026', 0

cmd_help    db 'help', 0
cmd_clear   db 'clear', 0
cmd_about   db 'about', 0
cmd_version db 'version', 0
cmd_gui     db 'gui', 0
cmd_reboot  db 'reboot', 0

; Mode 13h CRTC register pairs (index, value)
crtc13h:
    db 0x00,0x5F, 0x01,0x4F, 0x02,0x50, 0x03,0x82, 0x04,0x54
    db 0x05,0x80, 0x06,0xBF, 0x07,0x1F, 0x08,0x00, 0x09,0x41
    db 0x0A,0x00, 0x0B,0x00, 0x0C,0x00, 0x0D,0x00, 0x10,0x9C
    db 0x11,0x8E, 0x12,0x8F, 0x13,0x28, 0x14,0x40, 0x15,0x96
    db 0x16,0xB9, 0x17,0xA3, 0x18,0xFF, 0x0E,0x00, 0x0F,0x00

; Mode 13h GC register pairs
gc13h:
    db 0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00
    db 0x05,0x40, 0x06,0x05, 0x07,0x0F, 0x08,0xFF

; Mode 13h AC register pairs (index|0x20, value)
ac13h:
    db 0x00,0x00, 0x01,0x01, 0x02,0x02, 0x03,0x03, 0x04,0x04
    db 0x05,0x05, 0x06,0x06, 0x07,0x07, 0x08,0x08, 0x09,0x09
    db 0x0A,0x0A, 0x0B,0x0B, 0x0C,0x0C, 0x0D,0x0D, 0x0E,0x0E
    db 0x0F,0x0F, 0x10,0x41, 0x11,0x00, 0x12,0x0F, 0x13,0x00
    db 0x14,0x00

; Text mode GC pairs
gctext:
    db 0x00,0x00, 0x01,0x00, 0x02,0x00, 0x03,0x00, 0x04,0x00
    db 0x05,0x10, 0x06,0x0E, 0x07,0x00, 0x08,0xFF

dc_color db 0
dc_char  db 0
dc_bits  db 0
dc_basex dd 0
dc_basey dd 0
dc_glyph dd 0
dc_row   dd 0
dc_col   dd 0

gui_title db 'SQ-OS DESKTOP v2.0',0
lbl_files db 'FILES',0
lbl_term  db 'TERMINAL',0
lbl_set   db 'SETTINGS',0
lbl_esc   db 'ESC=SHELL',0

; 8x8 bitmap font, ASCII 32-90
font8x8:
    db 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 ; 32 SPACE
    db 0x18,0x18,0x18,0x18,0x00,0x18,0x00,0x00 ; 33 !
    db 0x66,0x66,0x44,0x00,0x00,0x00,0x00,0x00 ; 34 "
    db 0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00 ; 35 #
    db 0x7C,0x12,0x7C,0x48,0x7C,0x00,0x00,0x00 ; 36 $
    db 0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00 ; 37 %
    db 0x1C,0x22,0x1C,0x2A,0x44,0x3A,0x00,0x00 ; 38 &
    db 0x0C,0x08,0x10,0x00,0x00,0x00,0x00,0x00 ; 39 '
    db 0x0C,0x10,0x20,0x20,0x20,0x10,0x0C,0x00 ; 40 (
    db 0x30,0x08,0x04,0x04,0x04,0x08,0x30,0x00 ; 41 )
    db 0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00 ; 42 *
    db 0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00 ; 43 +
    db 0x00,0x00,0x00,0x00,0x18,0x18,0x10,0x20 ; 44 ,
    db 0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00 ; 45 -
    db 0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00 ; 46 .
    db 0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00 ; 47 /
    db 0x3C,0x42,0x46,0x4A,0x52,0x62,0x3C,0x00 ; 48 0
    db 0x18,0x28,0x08,0x08,0x08,0x08,0x3C,0x00 ; 49 1
    db 0x3C,0x42,0x02,0x0C,0x30,0x40,0x7E,0x00 ; 50 2
    db 0x7E,0x02,0x04,0x1C,0x02,0x42,0x3C,0x00 ; 51 3
    db 0x08,0x18,0x28,0x48,0x7E,0x08,0x08,0x00 ; 52 4
    db 0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00 ; 53 5
    db 0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00 ; 54 6
    db 0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00 ; 55 7
    db 0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00 ; 56 8
    db 0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00 ; 57 9
    db 0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00 ; 58 :
    db 0x00,0x18,0x18,0x00,0x18,0x10,0x20,0x00 ; 59 ;
    db 0x04,0x08,0x10,0x20,0x10,0x08,0x04,0x00 ; 60 <
    db 0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00 ; 61 =
    db 0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00 ; 62 >
    db 0x3C,0x42,0x04,0x08,0x00,0x08,0x00,0x00 ; 63 ?
    db 0x3C,0x42,0x4E,0x52,0x4E,0x40,0x3C,0x00 ; 64 @
    db 0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00 ; 65 A
    db 0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00 ; 66 B
    db 0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00 ; 67 C
    db 0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00 ; 68 D
    db 0x7E,0x40,0x40,0x78,0x40,0x40,0x7E,0x00 ; 69 E
    db 0x7E,0x40,0x40,0x78,0x40,0x40,0x40,0x00 ; 70 F
    db 0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00 ; 71 G
    db 0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00 ; 72 H
    db 0x3E,0x08,0x08,0x08,0x08,0x08,0x3E,0x00 ; 73 I
    db 0x04,0x04,0x04,0x04,0x04,0x44,0x38,0x00 ; 74 J
    db 0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00 ; 75 K
    db 0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00 ; 76 L
    db 0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00 ; 77 M
    db 0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00 ; 78 N
    db 0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00 ; 79 O
    db 0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00 ; 80 P
    db 0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00 ; 81 Q
    db 0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00 ; 82 R
    db 0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00 ; 83 S
    db 0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00 ; 84 T
    db 0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00 ; 85 U
    db 0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00 ; 86 V
    db 0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00 ; 87 W
    db 0x42,0x24,0x24,0x18,0x24,0x24,0x42,0x00 ; 88 X
    db 0x42,0x42,0x24,0x18,0x18,0x18,0x18,0x00 ; 89 Y
    db 0x7E,0x04,0x08,0x10,0x20,0x40,0x7E,0x00 ; 90 Z

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
