cpu 586
[org 0x7e00]

; =====================================================================
; STAGE 2 KERNEL ENTRY (Loaded at 0x7e00)
; =====================================================================

; Memory offsets for cursor and input buffers (safely out of code way)
cursor equ 0x9e00
input  equ 0x9e02
history equ 0x9e80

kernel_start:
    ; Setup segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00      ; Stack grows downwards from 0x7c00

    ; Clear text screen and draw initial UI
    mov ax, 0xb800
    mov es, ax
    call clear
    call draw_ui
    mov word [cursor], 1280

    ; --- Boot Animation ---
    ; Delay 1.5 seconds first
    mov cx, 0x0016
    mov dx, 0xE360
    mov ah, 0x86
    int 0x15

    mov di, [cursor]
    mov si, bootmsg1
    call print
    ; delay 500ms
    mov cx, 0x0007
    mov dx, 0xA120
    mov ah, 0x86
    int 0x15
    call newline_cursor

    mov di, [cursor]
    mov si, bootmsg2
    call print
    ; delay 500ms
    mov cx, 0x0007
    mov dx, 0xA120
    mov ah, 0x86
    int 0x15
    call newline_cursor

    mov di, [cursor]
    mov si, bootmsg3
    call print
    ; delay 1 second
    mov cx, 0x000F
    mov dx, 0x4240
    mov ah, 0x86
    int 0x15
    call newline_cursor

    call newline_cursor
    mov di, [cursor]
    mov si, loadmsg
    call print

    call newline_cursor
    mov di, [cursor]
    mov si, barmsg
    call print

    mov di, [cursor]
    sub di, 22

    mov cx, 10
.loadloop:
    mov ax, 0x0FDB      ; Block character
    stosw
    push cx
    ; delay 150ms
    mov cx, 0x0002
    mov dx, 0x49F0
    mov ah, 0x86
    int 0x15
    pop cx
    loop .loadloop

    ; delay 1 second
    mov cx, 0x000F
    mov dx, 0x4240
    mov ah, 0x86
    int 0x15
    call newline_cursor

    ; =====================================================================
    ; SEMIQUANTUM BOOT SPLASH (Mode 13h)
    ; =====================================================================
    mov ax, 0x0013
    int 0x10

    mov ax, 0xA000
    mov es, ax

    ; Black background
    mov di, 0
    mov cx, 64000
    mov al, 0
    rep stosb

    ; SQ Cyan logo block (centered)
    mov di, 22000
    mov cx, 5000
    mov al, 11
    rep stosb

    ; Cyan inner highlight (smaller centered box)
    mov di, 22700
    mov cx, 3600
    mov al, 3
    rep stosb

    ; Loading bar background (gray)
    mov di, 29500
    mov cx, 2000
    mov al, 8
    rep stosb

    ; Animate loading bar
    mov di, 29580
    mov cx, 10
.splash_barloop:
    push cx
    mov cx, 150
    mov al, 11          ; Cyan block fill
    rep stosb
    ; delay 150ms
    mov cx, 0x0002
    mov dx, 0x49F0
    mov ah, 0x86
    int 0x15
    pop cx
    loop .splash_barloop

    ; Print 'SemiQuantum' text using BIOS teletype
    mov ah, 0x02
    mov bh, 0
    mov dh, 12
    mov dl, 14
    int 0x10
    mov si, splashmsg
.splash_print:
    lodsb
    or al, al
    jz .splash_done
    mov ah, 0x0E
    int 0x10
    jmp .splash_print
.splash_done:

    ; Hold splash for 2 seconds
    mov cx, 0x001E
    mov dx, 0x8480
    mov ah, 0x86
    int 0x15

    ; Restore Text Mode
    mov ax, 0x0003
    int 0x10
    mov ax, 0xb800
    mov es, ax

    ; =====================================================================
    ; USER AUTHENTICATION
    ; =====================================================================
auth_start:
    jmp .auth_success
    call clear
    call draw_ui

    mov word [cursor], 320
    mov di, [cursor]
    mov si, bannermsg
    call print
    call newline_cursor

.retry_user:
    mov di, [cursor]
    mov si, userprompt
    call print
    mov byte [is_password], 0
    call get_input

    mov si, input
    mov di, expected_user
    call strcmp
    jnc .auth_fail

.retry_pass:
    call newline_cursor
    mov di, [cursor]
    mov si, passprompt
    call print
    mov byte [is_password], 1
    call get_input

    mov si, input
    mov di, expected_pass
    call strcmp
    jnc .auth_fail
    jmp .auth_success

.auth_fail:
    call newline_cursor
    mov di, [cursor]
    mov si, authfailmsg
    call print
    ; delay 1 second
    mov cx, 0x000F
    mov dx, 0x4240
    mov ah, 0x86
    int 0x15
    jmp auth_start

.auth_success:
    call clear
    call draw_ui

    mov word [cursor], 320
    mov di, [cursor]
    mov si, bannermsg
    call print
    call newline_cursor

    mov di, [cursor]
    mov si, authokmsg
    call print

    ; delay 1 second
    mov cx, 0x000F
    mov dx, 0x4240
    mov ah, 0x86
    int 0x15
    mov byte [is_password], 0

    call newline_cursor
    mov bx, input
    jmp shell_start


; =====================================================================
; AUTHENTICATION & INITIALIZATION DATA
; =====================================================================

userprompt      db 'Username: ',0
passprompt      db 'Password: ',0
expected_user   db 'harsh',0
expected_pass   db 'cto',0

bootmsg1        db '[OK] Initializing Memory',0
bootmsg2        db '[OK] Loading Kernel',0
bootmsg3        db '[OK] Starting SQ Services',0
loadmsg         db 'Loading SQ OS...',0
barmsg          db '[          ]',0
authfailmsg     db 'ACCESS DENIED',0
authokmsg       db 'WELCOME HARSH',0
splashmsg       db 'SemiQuantum OS',0


; =====================================================================
; MODULE INCLUSIONS (Included at end to avoid jump-fallthrough)
; =====================================================================

%include "graphics.asm"
%include "keyboard.asm"
%include "shell.asm"

; Pad kernel to exactly 30 sectors (15360 bytes)
times 15360-($-$$) db 0
