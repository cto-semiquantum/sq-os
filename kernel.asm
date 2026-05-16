cpu 586
[org 0x7e00]

times 510-($-$$) db 0
dw 0xaa55

; =================
; STAGE 2: KERNEL (Sector 2+)
; =================
cursor equ 0x9e00
input  equ 0x9e02
history equ 0x9e80

kernel_start:
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
mov ax, 0x0FDB
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

; =========================
; SEMIQUANTUM BOOT SPLASH
; =========================
mov ax, 0x0013
int 0x10

mov ax, 0xA000
mov es, ax

; Black background
mov di, 0
mov cx, 64000
mov al, 0
.splash_bg:
stosb
loop .splash_bg

; SQ Cyan logo block (centered)
mov di, 22000
mov cx, 5000
mov al, 11
.splash_logo:
stosb
loop .splash_logo

; Cyan inner highlight (smaller centered box)
mov di, 22700
mov cx, 3600
mov al, 3
.splash_inner:
stosb
loop .splash_inner

; Loading bar background (gray)
mov di, 29500
mov cx, 2000
mov al, 8
.splash_barbg:
stosb
loop .splash_barbg

; Animate loading bar
mov di, 29580
mov cx, 10
.splash_barloop:
push cx
mov bx, cx
mov cx, 150
mov al, 11
.splash_barfill:
stosb
loop .splash_barfill
mov di, di
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

; --- Authentication ---
auth_start:
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
jmp main

main:
mov di, [cursor]
mov si, prompt
call print
call get_input

mov bx, handlers
mov di, commands
.next_cmd:
cmp byte [di], 0
je nothelp
mov si, input
call strcmp
jc .match
.skip:
cmp byte [di-1], 0
je .next_handler
.skip_loop:
inc di
cmp byte [di-1], 0
jne .skip_loop
.next_handler:
add bx, 2
jmp .next_cmd
.match:
jmp word [bx]

get_input:
mov bx, input
.loop:
mov ah, 0
int 0x16
cmp al, 13
je .enter
cmp al, 8
je .bs
cmp ah, 0x48 ; UP arrow
je .up_arrow

mov di, [cursor]
mov [bx], al
inc bx
cmp byte [is_password], 1
je .star
mov ah, [textcolor]
stosw
jmp .store
.star:
mov al, '*'
mov ah, [textcolor]
stosw
.store:
mov [cursor], di
jmp .loop

.bs:
cmp bx, input
jle .loop
sub word [cursor], 2
dec bx
mov di, [cursor]
mov ah, [textcolor]
mov al, ' '
stosw
jmp .loop

.up_arrow:
cmp byte [is_password], 1
je .loop
.clear_input:
cmp bx, input
jle .load_hist
sub word [cursor], 2
dec bx
mov di, [cursor]
mov ah, [textcolor]
mov al, ' '
stosw
jmp .clear_input
.load_hist:
mov si, history
.print_hist:
lodsb
or al, al
je .loop
mov [bx], al
inc bx
mov ah, [textcolor]
mov di, [cursor]
stosw
mov [cursor], di
jmp .print_hist

.enter:
mov byte [bx], 0
cmp byte [is_password], 1
je .done_input
mov si, input
mov di, history
.copy_hist:
lodsb
stosb
or al, al
jne .copy_hist
.done_input:
ret

showhelp:
call newline_cursor
mov di, [cursor]
mov si, commands
.loop:
lodsb
or al, al
jz .space
mov ah, [textcolor]
stosw
jmp .loop
.space:
mov ax, 0x0b20
mov ah, [textcolor]
stosw
cmp byte [si], 0
jne .loop
mov [cursor], di
jmp print_msg_command_end

showversion:
mov si, versionmsg
jmp print_msg_command

showwhoami:
mov si, whoamimsg
jmp print_msg_command

showecho:
mov si, echomsg
jmp print_msg_command

showcalc:
mov si, calcmsg
jmp print_msg_command

showuname:
mov si, unamemsg
jmp print_msg_command

showexit:
mov si, exitmsg
jmp print_msg_command

showneofetch:
mov si, neofetchmsg
jmp print_msg_command

showbanner:
mov si, bannermsg
jmp print_msg_command

showdir:
mov si, dirmsg
jmp print_msg_command

showcd:
mov si, cdmsg
jmp print_msg_command

showcat:
mov si, catmsg
jmp print_msg_command

doclrg: mov byte [textcolor], 0x0A
jmp doclr_end
doclrb: mov byte [textcolor], 0x09
jmp doclr_end
doclrr: mov byte [textcolor], 0x0C
jmp doclr_end
doclrrst: mov byte [textcolor], 0x0F
jmp doclr_end
doclr_end:
call newline_cursor
jmp main

doclear:
call clear
call draw_ui
mov word [cursor], 1280
jmp main

docls:
call clear
mov word [cursor], 160
jmp main

doreboot:
int 0x19

doshutdown:
mov ax, 0x5307
mov bx, 0x0001
mov cx, 0x0003
int 0x15

dobeep:
in al, 0x61
or al, 3
out 0x61, al
mov al, 182
out 0x43, al
mov ax, 2000
out 0x42, al
mov al, ah
out 0x42, al
mov cx, 0x0003
mov dx, 0x0D40
mov ah, 0x86
int 0x15
in al, 0x61
and al, 0xFC
out 0x61, al
jmp print_msg_command_end

nothelp:
call newline_cursor
jmp main

showdate:
call newline_cursor
mov di, [cursor]
mov ah, 04h
int 1ah
mov al, dl
call print_bcd
mov ax, 0x0f2f
mov ah, [textcolor]
stosw
mov al, dh
call print_bcd
mov ax, 0x0f2f
mov ah, [textcolor]
stosw
mov al, ch
call print_bcd
mov al, cl
call print_bcd
mov [cursor], di
jmp print_msg_command_end

showtime:
call newline_cursor
mov di, [cursor]
mov ah, 02h
int 1ah
mov al, ch
call print_bcd
mov ax, 0x0f3a
mov ah, [textcolor]
stosw
mov al, cl
call print_bcd
mov ax, 0x0f3a
mov ah, [textcolor]
stosw
mov al, dh
call print_bcd
mov [cursor], di
jmp print_msg_command_end


showmem:
call newline_cursor
mov di, [cursor]
mov si, memmsg
call print
int 0x12
mov di, num_buffer
call itoa
mov si, num_buffer
call print
mov si, kbmsg
call print
jmp print_msg_command_end

showcpu:
call newline_cursor
mov di, [cursor]
mov si, cpumsg
call print
mov eax, 0
cpuid
mov dword [vendor_str], ebx
mov dword [vendor_str+4], edx
mov dword [vendor_str+8], ecx
mov byte [vendor_str+12], 0
mov si, vendor_str
call print
jmp print_msg_command_end

itoa:
pusha
mov cx, 0
mov bx, 10
.divide:
mov dx, 0
div bx
push dx
inc cx
cmp ax, 0
jne .divide
.pop:
pop dx
add dl, '0'
mov [di], dl
inc di
loop .pop
mov byte [di], 0
popa
ret


showgui:
mov ax, 0x0013
int 0x10

mov ax, 0xA000
mov es, ax

mov di, 0
mov cx, 64000
mov al, 0
.drawbg:
stosb
loop .drawbg

; =========================
; SQ LOGO
; =========================
mov di, 22000
mov cx, 5000
mov al, 11
.sqlogo:
stosb
loop .sqlogo

mov di, 0
mov cx, 3200
mov al, 8
.topbar:
stosb
loop .topbar

; =========================
; WINDOW
; =========================
mov di, 12000
mov cx, 12000
mov al, 7
.window:
stosb
loop .window

; =========================
; WINDOW TITLE BAR
; =========================
mov di, 12000
mov cx, 800
mov al, 15
.titlebar:
stosb
loop .titlebar

; =========================
; TASKBAR
; =========================
mov di, 60800
mov cx, 3200
mov al, 8
.taskbar:
stosb
loop .taskbar

; =========================
; START BUTTON
; =========================
mov di, 60820
mov cx, 400
mov al, 15
.startbtn:
stosb
loop .startbtn

; =========================
; TEXT RENDERING
; =========================
mov ah, 0x02
mov bh, 0
mov dh, 1
mov dl, 1
int 0x10

mov si, guimsg
.printmsg:
lodsb
or al, al
jz .done
mov ah, 0x0E
int 0x10
jmp .printmsg
.done:

; =========================
; DESKTOP ICON 1
; =========================
mov di, 7000
mov cx, 400
mov al, 15
.icon1:
stosb
loop .icon1

; =========================
; DESKTOP ICON 2
; =========================
mov di, 12000
mov cx, 400
mov al, 14
.icon2:
stosb
loop .icon2

; =========================
; DESKTOP ICON 3
; =========================
mov di, 17000
mov cx, 400
mov al, 12
.icon3:
stosb
loop .icon3

mov ah, 0

int 0x16

mov ax, 0x0003
int 0x10

mov ax, 0xb800
mov es, ax

call clear
call draw_ui
mov word [cursor], 1280
jmp main

showmouse:
; Init PS/2 mouse
mov ax, 0x0000
int 0x33
; Show hardware mouse cursor
mov ax, 0x0001
int 0x33

call newline_cursor
mov di, [cursor]
mov si, mouseinitmsg
call print
call newline_cursor

; Poll loop - read mouse position until ESC pressed
.mouseloop:
; Check keyboard for ESC (non-blocking)
mov ah, 0x01
int 0x16
jz .readmouse
; Key was pressed - check if ESC
mov ah, 0x00
int 0x16
cmp ah, 0x01   ; ESC scancode
je .mousedone

.readmouse:
; Get mouse position - int 33h AX=3
mov ax, 0x0003
int 0x33
; BX = buttons, CX = X, DX = Y

; Print on a fixed line so it refreshes
push cx
push dx
mov ah, 0x02
mov bh, 0
mov dh, 14
mov dl, 0
int 0x10

; Print "X:"
mov si, mouseXmsg
.px:
lodsb
or al, al
jz .px_done
mov ah, 0x0E
int 0x10
jmp .px
.px_done:

; Print X value (CX)
pop dx
pop cx
push cx
push dx
mov ax, cx
call print_mouse_num

; Print " Y:"
mov si, mouseYmsg
.py:
lodsb
or al, al
jz .py_done
mov ah, 0x0E
int 0x10
jmp .py
.py_done:

; Print Y value (DX)
pop dx
pop cx
mov ax, dx
call print_mouse_num

; Small delay
mov ah, 0x86
mov cx, 0
mov dx, 0x8000
int 0x15

jmp .mouseloop

.mousedone:
; Hide mouse cursor
mov ax, 0x0002
int 0x33
call newline_cursor
jmp main

; Print a number in AX using BIOS teletype
print_mouse_num:
push cx
mov cx, 0
.divide:
mov dx, 0
mov bx, 10
div bx
push dx
inc cx
cmp ax, 0
jne .divide
.pop_digit:
pop dx
add dl, '0'
mov ah, 0x0E
int 0x10
loop .pop_digit
; Print spaces to clear leftover digits
mov al, ' '
mov ah, 0x0E
int 0x10
int 0x10
int 0x10
pop cx
ret

print_msg_command:
call newline_cursor
mov di, [cursor]
call print
print_msg_command_end:
call newline_cursor
jmp main

print_bcd:
push ax
shr al, 4
call .nibble
pop ax
.nibble:
and al, 0x0f
add al, '0'
mov ah, [textcolor]
stosw
ret

strcmp:
.loop:
lodsb
mov ah, [di]
inc di
cmp al, ah
jne .fail
cmp al, 0
je .match
jmp .loop
.fail:
clc
ret
.match:
stc
ret

print:
.next:
lodsb
or al, al
jz .done
cmp al, 10
je .newline
mov ah, [textcolor]
stosw
jmp .next
.newline:
call newline_cursor
mov di, [cursor]
jmp .next
.done:
mov [cursor], di
ret

newline_cursor:
mov ax, [cursor]
mov bl, 160
div bl
inc al
mul bl
mov [cursor], ax
ret

draw_ui:
mov di, 160
mov si, versionmsg
call print
mov di, 640
mov si, logs
jmp print

clear:
xor di, di
mov cx, 2000
mov ah, [textcolor]
mov al, 0x20
rep stosw
ret

; =================
; DATA SECTION
; =================
commands db 'help',0,'clear',0,'about',0,'reboot',0,'shutdown',0,'version',0,'whoami',0,'date',0,'time',0,'echo',0,'cls',0,'calc',0,'uname',0,'exit',0,'neofetch',0,'color green',0,'color blue',0,'color red',0,'color reset',0,'beep',0,'banner',0,'dir',0,'cd',0,'cat',0,'mem',0,'cpu',0,'gui',0,'mouse',0,0
handlers dw showhelp, doclear, showversion, doreboot, doshutdown, showversion, showwhoami, showdate, showtime, showecho, docls, showcalc, showuname, showexit, showneofetch, doclrg, doclrb, doclrr, doclrrst, dobeep, showbanner, showdir, showcd, showcat, showmem, showcpu, showgui, showmouse

textcolor db 0x0f
is_password db 0

userprompt db 'Username: ',0
passprompt db 'Password: ',0
expected_user db 'harsh',0
expected_pass db 'cto',0
bootmsg1 db '[OK] Initializing Memory',0
bootmsg2 db '[OK] Loading Kernel',0
bootmsg3 db '[OK] Starting SQ Services',0
loadmsg db 'Loading SQ OS...',0
barmsg db '[          ]',0
authfailmsg db 'ACCESS DENIED',0
authokmsg db 'WELCOME HARSH',0

neofetchmsg db '   ___     OS: SQ OS',10
            db '  / _ \    Kernel: v1.0',10
            db ' | | | |   Arch: x86',10
            db ' | |_| |   Shell: SQSH',10
            db '  \___/ ',0

bannermsg db '  ____   ___     ___  ____  ',10
          db ' / ___| / _ \   / _ \/ ___| ',10
          db ' \___ \| | | | | | | \___ \ ',10
          db '  ___) | |_| | | |_| |___) |',10
          db ' |____/ \__\_\  \___/|____/ ',0

dirmsg db 'KERNEL.BIN   BOOT.BIN   CONFIG.SYS',0
cdmsg db 'Access Denied: Root Directory Locked',0
catmsg db 'Usage: cat [filename]',0


memmsg db 'Base Memory: ',0
kbmsg db ' KB',0
cpumsg db 'Processor: ',0
num_buffer db 0,0,0,0,0,0,0,0
vendor_str db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

mouseinitmsg db 'Mouse active. Move cursor. Press ESC to exit.',0
mouseXmsg db 'X: ',0
mouseYmsg db '  Y: ',0
guimsg db 'SemiQuantum',0
splashmsg db 'SemiQuantum OS',0
versionmsg db 'SQ OS v1.0',0
whoamimsg db 'harsh-cto',0
echomsg db 'Echo working',0
calcmsg db '2+2=4',0
unamemsg db 'SQOS x86',0
exitmsg db 'Cannot exit kernel shell',0
logs db 'System Ready',0
prompt db '> ',0

times 10240-($-$$) db 0
