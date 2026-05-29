; =====================================================================
; GRAPHICS & VGA SUBSYSTEM MODULE
; Handles Text-mode VGA (0xB800) and Mode 13h Graphics (0xA000)
; =====================================================================

; --- Variables & Configurations ---
textcolor db 0x0F
rect_color db 0

; =====================================================================
; TEXT MODE VGA FUNCTIONS (0xB800 segment)
; =====================================================================

; Clear text screen
clear:
    push es
    push di
    push cx
    push ax
    mov ax, 0xb800
    mov es, ax
    xor di, di
    mov cx, 2000
    mov ah, [textcolor]
    mov al, 0x20        ; Space character
    rep stosw
    pop ax
    pop cx
    pop di
    pop es
    ret

; Draw Text Mode UI (header and logs bar)
draw_ui:
    push di
    push si
    mov di, 160
    mov si, versionmsg
    call print
    mov di, 640
    mov si, logs
    call print
    pop si
    pop di
    ret

; Print a null-terminated string to the cursor position
print:
    pusha
.next:
    lodsb
    or al, al
    jz .done
    cmp al, 10          ; Newline character
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
    popa
    ret

; Advance text mode cursor to the next line
newline_cursor:
    push ax
    push bx
    mov ax, [cursor]
    mov bl, 160
    div bl
    inc al
    mul bl
    mov [cursor], ax
    pop bx
    pop ax
    ret

; Print BCD number (used by date/time commands)
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

; Integer to ASCII conversion (decimal)
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

; Print coordinate number for mouse polling (using BIOS teletype)
print_mouse_num:
    push cx
    push ax
    push bx
    push dx
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
    pop dx
    pop bx
    pop ax
    pop cx
    ret


; =====================================================================
; VGA MODE 13h GRAPHICS FUNCTIONS (0xA000 segment, 320x200 256-color)
; =====================================================================

; Clear Mode 13h screen with color in AL
clear_screen:
    push es
    push di
    push cx
    cld
    mov cx, 0xA000
    mov es, cx
    xor di, di
    mov cx, 64000
    rep stosb
    pop cx
    pop di
    pop es
    ret

; Draw a single pixel
;   cx = x (0-319)
;   dx = y (0-199)
;   al = color (0-255)
draw_pixel:
    push ds
    push es
    pusha

    ; Boundary checks
    cmp cx, 320
    jae .out_of_bounds
    cmp dx, 200
    jae .out_of_bounds

    mov bx, 0xA000
    mov es, bx

    ; Offset = Y * 320 + X
    ; Optimization: Y * 320 = (Y << 8) + (Y << 6)
    mov ax, dx
    mov di, dx
    shl di, 8
    shl ax, 6
    add di, ax
    add di, cx          ; di = Y * 320 + X

    ; Write color
    mov [es:di], al

.out_of_bounds:
    popa
    pop es
    pop ds
    ret

; Draw a filled rectangle
;   cx = x
;   dx = y
;   si = width
;   di = height
;   al = color
draw_rect:
    push ds
    push es
    pusha
    cld

    mov [rect_color], al
    mov ax, 0xA000
    mov es, ax

    xor bp, bp          ; bp = row index (0 to height-1)
.row_loop:
    cmp bp, di          ; compare row index to height
    jae .rect_done

    mov ax, dx          ; Y start
    add ax, bp          ; current Y
    cmp ax, 200         ; Y boundary check
    jae .rect_done

    ; Offset = current_Y * 320 + X
    ; current_Y * 320 = (current_Y << 8) + (current_Y << 6)
    mov bx, ax          ; bx = current Y
    shl bx, 8           ; bx = current Y * 256
    shl ax, 6           ; ax = current Y * 64
    add bx, ax          ; bx = current Y * 320
    add bx, cx          ; bx = current Y * 320 + X (offset)

    ; Draw the row line using rep stosb
    push di
    push cx
    mov di, bx          ; es:di = start offset
    mov cx, si          ; cx = width
    mov al, [rect_color]
    rep stosb
    pop cx
    pop di

.next_row:
    inc bp
    jmp .row_loop

.rect_done:
    popa
    pop es
    pop ds
    ret

; Render an 8x8 character using the bitmap font
;   cx = x
;   dx = y
;   al = ASCII character
;   ah = color
draw_char:
    pusha
    push ds
    push es
    cld

    mov bl, ah          ; bl = color

    ; Map lowercase to uppercase
    cmp al, 'a'
    jb .not_lower
    cmp al, 'z'
    ja .not_lower
    sub al, 32
.not_lower:

    ; Printable range check (32 to 90)
    cmp al, 32
    jb .done
    cmp al, 90
    ja .done

    ; Get offset of glyph: (char - 32) * 8
    sub al, 32
    xor ah, ah
    shl ax, 3           ; ax = (char - 32) * 8
    mov si, font8x8
    add si, ax          ; si = glyph data pointer

    mov ax, 0xA000
    mov es, ax

    xor bp, bp          ; bp = row counter (0 to 7)
.row_loop:
    cmp bp, 8
    jae .done

    ; Base offset calculation for Y coordinate: Y_dest = Y_start + row
    mov ax, dx
    add ax, bp
    cmp ax, 200         ; Y boundary check
    jae .next_row

    ; Offset = Y_dest * 320 + X_start
    mov di, ax
    shl di, 8
    shl ax, 6
    add di, ax
    add di, cx          ; di = screen destination offset

    ; Load glyph bitmap byte for this row
    mov al, [si + bp]   ; al = glyph row byte

    ; Loop through 8 bits (left to right)
    mov bh, 8
.bit_loop:
    test al, 0x80       ; check leftmost bit
    jz .pixel_skip

    ; Write pixel color
    mov [es:di], bl

.pixel_skip:
    inc di              ; advance screen X
    shl al, 1           ; shift next bit to MSB
    dec bh
    jnz .bit_loop

.next_row:
    inc bp
    jmp .row_loop

.done:
    pop es
    pop ds
    popa
    ret

; Draw a null-terminated string
;   si = string pointer
;   cx = x
;   dx = y
;   al = color
draw_text:
    pusha
    mov ah, al          ; ah = color for draw_char
.char_loop:
    lodsb
    or al, al
    jz .done
    call draw_char
    add cx, 8           ; move to next character column
    jmp .char_loop
.done:
    popa
    ret

; Draw a premium 3D-bevelled window
;   cx = x
;   dx = y
;   si = width
;   di = height
;   bp = title string pointer
draw_window:
    pusha

    ; 1. Draw Shadow (dark gray, shifted right/down by 3px)
    push cx
    push dx
    add cx, 3
    add dx, 3
    mov al, 8           ; Dark gray shadow
    call draw_rect
    pop dx
    pop cx

    ; 2. Draw Window Body (light gray)
    mov al, 7           ; Light gray
    call draw_rect

    ; 3. Draw 3D Bevel Borders
    ; Top Highlight (white)
    push di
    mov di, 1
    mov al, 15          ; White
    call draw_rect
    pop di

    ; Left Highlight (white)
    push si
    mov si, 1
    mov al, 15          ; White
    call draw_rect
    pop si

    ; Bottom Shadow (dark gray)
    push cx
    push dx
    push di
    add dx, di
    dec dx              ; Y + height - 1
    mov di, 1
    mov al, 8           ; Dark gray
    call draw_rect
    pop di
    pop dx
    pop cx

    ; Right Shadow (dark gray)
    push cx
    push dx
    push si
    add cx, si
    dec cx              ; X + width - 1
    mov si, 1
    mov al, 8           ; Dark gray
    call draw_rect
    pop si
    pop dx
    pop cx

    ; 4. Draw Title Bar (blue)
    push cx
    push dx
    push si
    push di
    add cx, 2
    add dx, 2
    sub si, 4
    mov di, 10          ; 10 pixels height
    mov al, 1           ; Dark blue
    call draw_rect
    pop di
    pop si
    pop dx
    pop cx

    ; 5. Draw Title Text (white)
    push cx
    push dx
    push si
    push di
    add cx, 6
    add dx, 3
    mov si, bp          ; si = title string pointer for draw_text
    mov al, 15          ; White text
    call draw_text
    pop di
    pop si
    pop dx
    pop cx

    ; 6. Draw Close Button (small red square)
    push cx
    push dx
    push si
    push di
    add cx, si
    sub cx, 11
    add dx, 3
    mov si, 6
    mov di, 6
    mov al, 12          ; Red
    call draw_rect
    pop di
    pop si
    pop dx
    pop cx

; =====================================================================
; GUI COMPONENT DRAWING ENGINE APIs
; =====================================================================

; Draw a 3D-bevelled button
;   cx = x
;   dx = y
;   si = width
;   di = height
;   bp = text string pointer
;   al = state (0 = normal, 1 = pressed)
draw_button:
    pusha
    mov bh, al          ; bh = state

    ; 1. Draw button background (light gray or dark gray based on state)
    cmp bh, 0
    jne .pressed_bg
    mov al, 7           ; Light gray
    jmp .draw_bg
.pressed_bg:
    mov al, 8           ; Dark gray (pressed background)
.draw_bg:
    call draw_rect

    ; 2. Draw 3D Bevel borders
    ; Normal state: top/left is white (15), bottom/right is dark gray (8)
    ; Pressed state: top/left is dark gray (8), bottom/right is white (15)
    cmp bh, 0
    jne .pressed_bevel

    ; Top highlight (white)
    push di
    mov di, 1
    mov al, 15
    call draw_rect
    pop di

    ; Left highlight (white)
    push si
    mov si, 1
    mov al, 15
    call draw_rect
    pop si

    ; Bottom shadow (dark gray)
    push cx
    push dx
    push di
    add dx, di
    dec dx
    mov di, 1
    mov al, 8
    call draw_rect
    pop di
    pop dx
    pop cx

    ; Right shadow (dark gray)
    push cx
    push dx
    push si
    add cx, si
    dec cx
    mov si, 1
    mov al, 8
    call draw_rect
    pop si
    pop dx
    pop cx
    jmp .draw_text

.pressed_bevel:
    ; Top shadow (dark gray)
    push di
    mov di, 1
    mov al, 8
    call draw_rect
    pop di

    ; Left shadow (dark gray)
    push si
    mov si, 1
    mov al, 8
    call draw_rect
    pop si

    ; Bottom highlight (white)
    push cx
    push dx
    push di
    add dx, di
    dec dx
    mov di, 1
    mov al, 15
    call draw_rect
    pop di
    pop dx
    pop cx

    ; Right highlight (white)
    push cx
    push dx
    push si
    add cx, si
    dec cx
    mov si, 1
    mov al, 15
    call draw_rect
    pop si
    pop dx
    pop cx

.draw_text:
    ; 3. Draw text in center of button
    push cx
    push dx
    add cx, 4           ; Small left padding
    mov ax, di          ; button height
    sub ax, 8
    shr ax, 1           ; ax = (height - 8) / 2
    add dx, ax
    mov si, bp          ; string pointer
    cmp bh, 0
    jne .pressed_text_color
    mov al, 0           ; Black text for normal state
    jmp .print_txt
.pressed_text_color:
    mov al, 15          ; White text for pressed state
.print_txt:
    call draw_text
    pop dx
    pop cx

    popa
    ret

; Draw a graphical icon with text centered below
;   cx = x
;   dx = y
;   al = icon type (0 = computer, 1 = folder, 2 = file)
;   bp = label text string pointer
draw_icon:
    pusha
    mov bh, al          ; bh = icon type

    ; 1. Draw graphical icon shape
    cmp bh, 0
    je .draw_computer
    cmp bh, 1
    je .draw_folder
    cmp bh, 2
    je .draw_file
    jmp .draw_label

.draw_computer:
    ; Monitor screen (cyan)
    push cx
    push dx
    add cx, 2
    mov si, 12
    mov di, 8
    mov al, 3           ; Cyan
    call draw_rect
    pop dx
    pop cx

    ; Monitor stand (dark gray base)
    push cx
    push dx
    add cx, 7
    add dx, 8
    mov si, 2
    mov di, 2
    mov al, 8           ; Dark gray
    call draw_rect
    pop dx
    pop cx

    push cx
    push dx
    add cx, 4
    add dx, 10
    mov si, 8
    mov di, 1
    mov al, 8           ; Dark gray base
    call draw_rect
    pop dx
    pop cx
    jmp .draw_label

.draw_folder:
    ; Folder back/body (yellow)
    push cx
    push dx
    mov si, 14
    mov di, 10
    mov al, 14          ; Yellow
    call draw_rect
    pop dx
    pop cx

    ; Folder front pocket highlight
    push cx
    push dx
    add cx, 1
    add dx, 2
    mov si, 12
    mov di, 7
    mov al, 6           ; Brown/Dark yellow
    call draw_rect
    pop dx
    pop cx
    jmp .draw_label

.draw_file:
    ; Document page (white)
    push cx
    push dx
    add cx, 2
    mov si, 10
    mov di, 12
    mov al, 15          ; White sheet
    call draw_rect
    pop dx
    pop cx

    ; Document text lines (dark gray)
    push cx
    push dx
    add cx, 4
    add dx, 3
    mov si, 6
    mov di, 1
    mov al, 8
    call draw_rect
    pop dx
    pop cx

    push cx
    push dx
    add cx, 4
    add dx, 6
    mov si, 6
    mov di, 1
    mov al, 8
    call draw_rect
    pop dx
    pop cx

    push cx
    push dx
    add cx, 4
    add dx, 9
    mov si, 4
    mov di, 1
    mov al, 8
    call draw_rect
    pop dx
    pop cx

.draw_label:
    ; 2. Center label under the icon
    push cx
    push dx
    add dx, 14          ; Y offset under icon
    sub cx, 8           ; shift X left to roughly center
    mov si, bp          ; string pointer
    mov al, 15          ; White text
    call draw_text
    pop dx
    pop cx

    popa
    ret

; Draw the bottom 3D taskbar (Y=190..199)
draw_taskbar:
    pusha

    ; Taskbar body (light gray)
    mov cx, 0
    mov dx, 190
    mov si, 320
    mov di, 10
    mov al, 7
    call draw_rect

    ; Top highlight bevel line (white)
    mov cx, 0
    mov dx, 190
    mov si, 320
    mov di, 1
    mov al, 15
    call draw_rect

    ; Start button (bevelled button using draw_button)
    mov cx, 4
    mov dx, 191
    mov si, 36
    mov di, 8
    mov bp, start_btn_txt
    mov al, 0           ; Normal state
    call draw_button

    popa
    ret


; =====================================================================
; GRAPHICAL MOUSE CURSOR DRIVER
; =====================================================================

; Save screen background under mouse cursor coordinate
;   cx = x, dx = y
save_mouse_bg:
    pusha
    push ds
    push es
    cld

    mov ax, 0xA000
    mov ds, ax          ; read from VGA segment
    xor ax, ax
    mov es, ax          ; write to data segment
    mov di, mouse_bg_buffer

    xor bp, bp          ; bp = row index (0 to 11)
.row_loop:
    cmp bp, 12
    jae .done

    mov ax, dx
    add ax, bp          ; screen Y
    cmp ax, 200
    jae .pad_row

    ; Calculate screen offset (Y * 320 + X)
    mov si, ax
    shl si, 8
    shl ax, 6
    add si, ax
    add si, cx          ; ds:si = source address

    xor bx, bx          ; bx = col index (0 to 7)
.col_loop:
    cmp bx, 8
    jae .next_row

    mov ax, cx
    add ax, bx
    cmp ax, 320
    jae .pad_col

    movsb               ; copy pixel from screen (ds:si) to buffer (es:di)
    jmp .next_col

.pad_col:
    mov byte [es:di], 0 ; fill out of bounds with black
    inc di
.next_col:
    inc bx
    jmp .col_loop

.pad_row:
    mov ecx, 8          ; fill whole row with black
    mov al, 0
    rep stosb
.next_row:
    inc bp
    jmp .row_loop

.done:
    pop es
    pop ds
    popa
    ret

; Restore screen background under old mouse coordinate
;   cx = x, dx = y
restore_mouse_bg:
    pusha
    push ds
    push es
    cld

    xor ax, ax
    mov ds, ax          ; read from data segment
    mov si, mouse_bg_buffer
    mov ax, 0xA000
    mov es, ax          ; write to VGA segment

    xor bp, bp          ; bp = row index (0 to 11)
.row_loop:
    cmp bp, 12
    jae .done

    mov ax, dx
    add ax, bp          ; Y coordinate
    cmp ax, 200
    jb .y_ok
    add si, 8
    jmp .next_row
.y_ok:

    ; Calculate screen offset (Y * 320 + X)
    mov di, ax
    shl di, 8
    shl ax, 6
    add di, ax
    add di, cx          ; es:di = screen offset

    xor bx, bx          ; bx = col index (0 to 7)
.col_loop:
    cmp bx, 8
    jae .next_row

    mov ax, cx
    add ax, bx
    cmp ax, 320
    jae .skip_pixel

    movsb               ; restore pixel from buffer (ds:si) to screen (es:di)
    jmp .next_col

.skip_pixel:
    inc si
.next_col:
    inc bx
    jmp .col_loop

.next_row:
    inc bp
    jmp .row_loop

.done:
    pop es
    pop ds
    popa
    ret

; Draw graphical mouse cursor using the 8x12 mouse_sprite at (cx, dx)
;   cx = x, dx = y
draw_mouse_cursor:
    pusha
    push ds
    push es
    cld

    xor ax, ax
    mov ds, ax          ; read from data segment
    mov si, mouse_sprite
    mov ax, 0xA000
    mov es, ax          ; write to VGA segment

    xor bp, bp          ; bp = row index (0 to 11)
.row_loop:
    cmp bp, 12
    jae .done

    mov ax, dx
    add ax, bp          ; screen Y
    cmp ax, 200
    jb .y_ok
    add si, 8
    inc bp
    jmp .row_loop
.y_ok:

    ; Calculate screen offset (Y * 320 + X)
    mov di, ax
    shl di, 8
    shl ax, 6
    add di, ax
    add di, cx          ; es:di = screen offset

    xor bx, bx          ; bx = col index (0 to 7)
.col_loop:
    cmp bx, 8
    jae .next_row

    mov ax, cx
    add ax, bx
    cmp ax, 320
    jae .skip_pixel

    ; Load sprite byte
    lodsb               ; AL = sprite byte, SI++
    cmp al, 0
    je .next_col        ; transparent
    cmp al, 1
    je .black
    
    ; color 15 (white)
    mov byte [es:di], 15
    jmp .next_col
.black:
    mov byte [es:di], 0
    jmp .next_col

.skip_pixel:
    inc si
.next_col:
    inc di
    inc bx
    jmp .col_loop

.next_row:
    inc bp
    jmp .row_loop

.done:
    pop es
    pop ds
    popa
    ret

; Perform mouse update (NOP under the new state-driven GUI loop architecture)
draw_mouse:
    ret

; Hide mouse cursor (NOP under the new state-driven GUI loop architecture)
hide_mouse:
    ret


; =====================================================================
; GRAPHICAL MOUSE STATE VARIABLES & DATA
; =====================================================================

mouse_visible       db 0
needs_redraw        db 1
mouse_x             dw 0
mouse_y             dw 0
last_mouse_buttons  db 0
desktop_win_open    db 1
explorer_win_open   db 0
start_menu_open     db 0
drag_active         db 0        ; 0 = none, 1 = welcome, 2 = explorer
drag_offset_x       dw 0
drag_offset_y       dw 0

win_welcome_x       dw 65
win_welcome_y       dw 35
win_explorer_x      dw 100
win_explorer_y      dw 60

start_btn_txt       db 'START',0
start_menu_title    db 'START',0
menu_about_txt      db 'ABOUT',0
menu_shell_txt      db 'SHELL',0
menu_reboot_txt     db 'REBOOT',0
menu_shut_txt       db 'SHUTDOWN',0

; 8x12 custom mouse cursor sprite
; 0 = transparent, 1 = black outline, 2 = white fill
mouse_sprite:
    db 1,1,0,0,0,0,0,0
    db 1,2,1,0,0,0,0,0
    db 1,2,2,1,0,0,0,0
    db 1,2,2,2,1,0,0,0
    db 1,2,2,2,2,1,0,0
    db 1,2,2,2,2,2,1,0
    db 1,2,2,2,2,2,2,1
    db 1,2,2,2,2,1,1,1
    db 1,2,1,1,2,1,0,0
    db 1,1,0,1,2,2,1,0
    db 0,0,0,0,1,2,2,1
    db 0,0,0,0,0,1,1,0

; 96-byte buffer (12 rows * 8 columns) to save screen pixels under mouse
mouse_bg_buffer times 96 db 0

; --- Font Include at the end ---
%include "fonts.asm"
