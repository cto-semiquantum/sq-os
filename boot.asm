[org 0x7c00]

cursor equ 0x7e00
input  equ 0x7e02

mov ax,0xb800
mov es,ax

call clear
call draw_ui

; =================
; PROMPT
; =================

mov word [cursor],1280

main:

mov di,[cursor]
mov si,prompt
call print

mov bx,input

input_loop:

mov ah,0
int 0x16

cmp al,13
je newline

cmp al,8
je backspace

mov di,[cursor]

mov [bx],al
inc bx

mov ah,0x0f
stosw

mov [cursor],di

jmp input_loop

backspace:

mov ax,[cursor]
cmp ax,1288
jle input_loop

sub word [cursor],2
dec bx
mov di,[cursor]
mov ax,0x0720
stosw

jmp input_loop

newline:

mov byte [bx],0

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
mov al, 0
.skip_loop:
scasb
jne .skip_loop
.next_handler:
add bx, 2
jmp .next_cmd
.match:
jmp word [bx]

showhelp:
mov si,helpmsg
jmp print_msg_command

showabout:
mov si,aboutmsg
jmp print_msg_command

showversion:
mov si,versionmsg
jmp print_msg_command

showwhoami:
mov si,whoamimsg
jmp print_msg_command

doclear:
call clear
call draw_ui
mov word [cursor],1280
mov bx,input
jmp main

doreboot:
int 0x19

nothelp:
call newline_cursor
mov bx,input
jmp main

; =================
; PRINT MSG COMMAND
; =================
print_msg_command:
call newline_cursor
mov di,[cursor]
call print
call newline_cursor
mov bx,input
jmp main

; =================
; STRCMP
; =================
strcmp:
.loop:
lodsb
mov ah,[di]
inc di
cmp al,ah
jne .fail
cmp al,0
je .match
jmp .loop
.fail:
clc
ret
.match:
stc
ret

; =================
; PRINT
; =================
print:
.next:
lodsb
or al,al
jz .done
cmp al, 10
je .newline
mov ah,0x0b
stosw
jmp .next
.newline:
call newline_cursor
mov di, [cursor]
jmp .next
.done:
mov [cursor],di
ret

; =================
; NEWLINE CURSOR
; =================
newline_cursor:
mov ax,[cursor]
mov bl,160
div bl
inc al
mul bl
mov [cursor],ax
ret

; =================
; DRAW UI
; =================
draw_ui:
mov di,160
mov si,versionmsg
call print

mov di,320
mov cx,20
mov ax,0x0b3d ; '='
rep stosw

mov di,640
mov si,logs
jmp print

; =================
; CLEAR SCREEN
; =================
clear:
xor di,di
mov cx,2000
mov ax,0x0720
rep stosw
ret

; =================
; DATA
; =================
commands db 'help',0,'clear',0,'about',0,'reboot',0,'version',0,'whoami',0,0
handlers dw showhelp, doclear, showabout, doreboot, showversion, showwhoami
aboutmsg db 'SQ OS by SemiQuantum',0
versionmsg db 'SQ OS Kernel v1.0',0
whoamimsg db 'semiquantum-user',0
helpmsg db 'help clear about reboot version whoami',0
logs db '[OK] Initializing System',10,'[OK] Loading Kernel',10,'[OK] System Ready',0
prompt db 'SQ> ',0

times 510-($-$$) db 0
dw 0xaa55