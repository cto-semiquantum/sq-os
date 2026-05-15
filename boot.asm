[org 0x7c00]

mov ax,0xb800
mov es,ax

call clear

; =================
; UI (LEFT ALIGNED)
; =================

mov di,160
mov si,title
call print

mov di,320
mov si,line
call print

mov di,640
mov si,log1
call print

mov di,800
mov si,log2
call print

mov di,960
mov si,log3
call print

; =================
; PROMPT
; =================

mov word [cursor],1280

main:

mov di,[cursor]
mov si,prompt
call print

input_loop:

mov ah,0
int 0x16

cmp al,13
je newline

mov di,[cursor]

mov ah,0x0f
stosw

mov [cursor],di

jmp input_loop

newline:

mov ax,[cursor]
add ax,160

mov bx,160
xor dx,dx
div bx
mul bx

mov [cursor],ax

jmp main

; =================
; PRINT
; =================

print:

.next:

lodsb
or al,al
jz .done

mov ah,0x0b
stosw

jmp .next

.done:

mov [cursor],di
ret

; =================
; CLEAR SCREEN
; =================

clear:

mov di,0
mov cx,2000

.loop:

mov ax,0x0720
stosw

loop .loop

ret

; =================
; DATA
; =================

cursor dw 0

title db 'SQ OS v1.0',0
line  db '====================',0

log1 db '[OK] Initializing System',0
log2 db '[OK] Loading Kernel',0
log3 db '[OK] System Ready',0

prompt db 'SQ> ',0

times 510-($-$$) db 0
dw 0xaa55