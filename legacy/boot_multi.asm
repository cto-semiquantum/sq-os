[org 0x7c00]

; --- SECTOR 1: BOOTLOADER ---
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x7c00

mov ah, 0x02      ; Read sectors
mov al, 10        ; Read 10 sectors (5KB)
mov ch, 0         ; Cylinder 0
mov cl, 2         ; Sector 2
mov dh, 0         ; Head 0
mov bx, 0x7e00    ; Load kernel at 0x7e00
int 0x13

jmp 0x7e00

times 510-($-$$) db 0
dw 0xaa55

; --- SECTOR 2+: KERNEL ---
; The kernel starts exactly at 0x7e00 because it's the second sector!
cursor equ 0x9e00 ; Move cursor/input safely out of the way
input  equ 0x9e02

kernel_start:
mov ax, 0xb800
mov es, ax

call clear
call draw_ui
mov word [cursor], 1280

main:
mov di, [cursor]
mov si, prompt
call print
mov bx, input

input_loop:
mov ah, 0
int 0x16
cmp al, 13
je newline
cmp al, 8
je backspace
mov di, [cursor]
mov [bx], al
inc bx
mov ah, 0x0f
stosw
mov [cursor], di
jmp input_loop

backspace:
mov ax, [cursor]
cmp ax, 1288
jle input_loop
sub word [cursor], 2
dec bx
mov di, [cursor]
mov ax, 0x0720
stosw
jmp input_loop

newline:
mov byte [bx], 0
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

showhelp:
call newline_cursor
mov di, [cursor]
mov si, commands
.loop:
lodsb
or al, al
jz .space
mov ah, 0x0b
stosw
jmp .loop
.space:
mov ax, 0x0b20
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

doclear:
call clear
call draw_ui
mov word [cursor], 1280
mov bx, input
jmp main

doreboot:
int 0x19

doshutdown:
mov ax, 0x5307
mov bx, 0x0001
mov cx, 0x0003
int 0x15

nothelp:
call newline_cursor
mov bx, input
jmp main

showdate:
call newline_cursor
mov di, [cursor]
mov ah, 04h
int 1ah
mov al, dl
call print_bcd
mov ax, 0x0f2f
stosw
mov al, dh
call print_bcd
mov ax, 0x0f2f
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
stosw
mov al, cl
call print_bcd
mov ax, 0x0f3a
stosw
mov al, dh
call print_bcd
mov [cursor], di
jmp print_msg_command_end

print_msg_command:
call newline_cursor
mov di, [cursor]
call print
print_msg_command_end:
call newline_cursor
mov bx, input
jmp main

print_bcd:
push ax
shr al, 4
call .nibble
pop ax
.nibble:
and al, 0x0f
add al, '0'
mov ah, 0x0f
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
mov ah, 0x0b
stosw
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
mov ax, 0x0720
rep stosw
ret

commands db 'help',0,'clear',0,'cls',0,'about',0,'reboot',0,'shutdown',0,'version',0,'whoami',0,'date',0,'time',0,'echo',0,'calc',0,'uname',0,'exit',0,0
handlers dw showhelp, doclear, doclear, showversion, doreboot, doshutdown, showversion, showwhoami, showdate, showtime, showecho, showcalc, showuname, showexit

versionmsg db 'SQ OS v1.0',0
whoamimsg db 'semiquantum-user',0
echomsg db 'Echo working',0
calcmsg db '2+2=4',0
unamemsg db 'SQOS x86',0
exitmsg db 'Cannot exit kernel shell',0
logs db 'System Ready',0
prompt db '> ',0

; Pad kernel to fill 10 sectors (so QEMU doesn't complain about reading past EOF)
times 5120-($-$$) db 0
