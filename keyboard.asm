; =====================================================================
; KEYBOARD & INPUT UTILITIES MODULE
; Handles Keyboard Input, Masking, and String Comparisons
; =====================================================================

; --- Variables & Configurations ---
is_password db 0

; Wait for keyboard input and store/echo it
get_input:
    pusha
    mov bx, input
.loop:
    mov ah, 0
    int 0x16            ; BIOS get keystroke (blocking)
    cmp al, 13          ; Enter key
    je .enter
    cmp al, 8           ; Backspace key
    je .bs
    cmp ah, 0x48        ; UP arrow key (history retrieval)
    je .up_arrow

    mov di, [cursor]
    mov [bx], al        ; Store character in input buffer
    inc bx
    cmp byte [is_password], 1
    je .star
    mov ah, [textcolor]
    stosw               ; Print character to text screen
    jmp .store
.star:
    mov al, '*'
    mov ah, [textcolor]
    stosw               ; Print asterisk for password mask
.store:
    mov [cursor], di
    jmp .loop

.bs:
    cmp bx, input       ; Cannot backspace past start of buffer
    jle .loop
    sub word [cursor], 2
    dec bx
    mov di, [cursor]
    mov ah, [textcolor]
    mov al, ' '         ; Erase character from screen
    stosw
    jmp .loop

.up_arrow:
    cmp byte [is_password], 1   ; Do not retrieve history for password
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
    mov byte [bx], 0    ; Null-terminate input buffer
    cmp byte [is_password], 1
    je .done_input
    
    ; Copy current input into history buffer for next retrieval
    mov si, input
    mov di, history
.copy_hist:
    lodsb
    stosb
    or al, al
    jne .copy_hist
.done_input:
    popa
    ret

; String Comparison
; Inputs: SI = string 1, DI = string 2
; Returns: Carry flag set if equal, cleared if not
strcmp:
    push si
    push di
    push ax
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
    pop ax
    pop di
    pop si
    clc                 ; Equal = False
    ret
.match:
    pop ax
    pop di
    pop si
    stc                 ; Equal = True
    ret
