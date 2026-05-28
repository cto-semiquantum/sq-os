; =============================================
; gui/window.asm
; Window structures, rendering APIs, and window manager logic
; =============================================

; ---- Window Structure Offsets ----
WIN_X       equ 0    ; dd
WIN_Y       equ 4    ; dd
WIN_W       equ 8    ; dd
WIN_H       equ 12   ; dd
WIN_TITLE   equ 16   ; dd (pointer to null-terminated string)
WIN_VISIBLE equ 20   ; dd (1 = visible, 0 = hidden)
WIN_ACTIVE  equ 24   ; dd (1 = active, 0 = inactive)
WIN_COLOR   equ 28   ; dd (VGA color index, e.g. 7)
WIN_SIZE    equ 32

; ---- draw_cursor_pm ----
; Draws a 5x5 white square with a black center dot at mouse position
draw_cursor_pm:
    pushad
    mov ebx, [mouse_x]
    mov ecx, [mouse_y]
    ; Clamp so 5x5 cursor stays fully on screen
    cmp ebx, 315
    jle .cx_ok
    mov ebx, 315
.cx_ok:
    cmp ecx, 195
    jle .cy_ok
    mov ecx, 195
.cy_ok:
    mov edx, 5
    mov esi, 5
    mov al, 15          ; white
    call draw_rect_pm
    ; Black inner pixel for crosshair feel
    mov ebx, [mouse_x]
    add ebx, 2
    mov ecx, [mouse_y]
    add ecx, 2
    mov edx, 1
    mov esi, 1
    mov al, 0           ; black center dot
    call draw_rect_pm
    popad
    ret

; ---- draw_all_windows ----
; Draws all windows in order of their Z-index (bottom to top)
draw_all_windows:
    pushad
    xor ecx, ecx
.loop:
    cmp ecx, 4          ; 4 windows in total
    jae .done
    mov esi, [window_order + ecx*4]
    call draw_window
    inc ecx
    jmp .loop
.done:
    popad
    ret

; ---- draw_window ----
; Draws a single window if it is marked visible
; Input: ESI = pointer to Window structure
draw_window:
    pushad
    cmp dword [esi + WIN_VISIBLE], 1
    jne .exit

    ; 1. Draw window border
    mov ebx, [esi + WIN_X]
    mov ecx, [esi + WIN_Y]
    mov edx, [esi + WIN_W]
    mov edi, [esi + WIN_H]
    mov al, [esi + WIN_COLOR]
    call draw_window_border

    ; 2. Draw title bar
    mov ebx, [esi + WIN_X]
    mov ecx, [esi + WIN_Y]
    mov edx, [esi + WIN_W]
    mov edi, [esi + WIN_TITLE]
    mov al, [esi + WIN_ACTIVE]
    call draw_titlebar

    ; 3. Draw close button
    mov ebx, [esi + WIN_X]
    mov ecx, [esi + WIN_Y]
    mov edx, [esi + WIN_W]
    call draw_close_button

    ; 4. Draw window contents based on ESI pointer
    cmp esi, window_welcome
    je .draw_welcome
    cmp esi, window_files
    je .draw_files
    cmp esi, window_terminal
    je .draw_terminal
    cmp esi, window_settings
    je .draw_settings
    jmp .exit

.draw_welcome:
    call draw_welcome_content_func
    jmp .exit
.draw_files:
    call draw_files_content_func
    jmp .exit
.draw_terminal:
    call draw_terminal_content_func
    jmp .exit
.draw_settings:
    call draw_settings_content_func

.exit:
    popad
    ret

; ---- draw_window_border ----
; Draws a Win95-style 3D bevelled window border
; Input: EBX=x, ECX=y, EDX=width, EDI=height, AL=color
draw_window_border:
    pushad
    mov [wb_x], ebx
    mov [wb_y], ecx
    mov [wb_w], edx
    mov [wb_h], edi
    mov [wb_c], al

    ; Body (light gray)
    mov ebx, [wb_x]
    mov ecx, [wb_y]
    mov edx, [wb_w]
    mov esi, [wb_h]
    mov al, [wb_c]
    call draw_rect_pm

    ; Top highlight (white)
    mov ebx, [wb_x]
    mov ecx, [wb_y]
    mov edx, [wb_w]
    mov esi, 1
    mov al, 15
    call draw_rect_pm

    ; Left highlight (white)
    mov ebx, [wb_x]
    mov ecx, [wb_y]
    mov edx, 1
    mov esi, [wb_h]
    mov al, 15
    call draw_rect_pm

    ; Bottom shadow (dark gray)
    mov ebx, [wb_x]
    mov ecx, [wb_y]
    add ecx, [wb_h]
    dec ecx
    mov edx, [wb_w]
    mov esi, 1
    mov al, 8
    call draw_rect_pm

    ; Right shadow (dark gray)
    mov ebx, [wb_x]
    add ebx, [wb_w]
    dec ebx
    mov ecx, [wb_y]
    mov edx, 1
    mov esi, [wb_h]
    mov al, 8
    call draw_rect_pm

    popad
    ret

; ---- draw_titlebar ----
; Draws the title bar (blue if active, dark gray if inactive)
; Input: EBX=x, ECX=y, EDX=width, EDI=title_str, AL=active
draw_titlebar:
    pushad
    mov [wt_x], ebx
    mov [wt_y], ecx
    mov [wt_w], edx
    mov [wt_title], edi
    mov [wt_active], al

    ; Set colors based on active state
    cmp byte [wt_active], 1
    je .active
    mov al, 8           ; Inactive title bar: dark gray
    mov byte [wt_text_color], 7 ; Inactive text: light gray
    jmp .draw_rect
.active:
    mov al, 1           ; Active title bar: blue
    mov byte [wt_text_color], 15 ; Active text: white

.draw_rect:
    mov ebx, [wt_x]
    add ebx, 2
    mov ecx, [wt_y]
    add ecx, 2
    mov edx, [wt_w]
    sub edx, 4
    mov esi, 10
    call draw_rect_pm

    ; Draw Title Text
    mov esi, [wt_title]
    mov ebx, [wt_x]
    add ebx, 6
    mov ecx, [wt_y]
    add ecx, 3
    mov dl, [wt_text_color]
    call draw_text

    popad
    ret

; ---- draw_close_button ----
; Draws an [X] close button
; Input: EBX=x, ECX=y, EDX=width
draw_close_button:
    pushad
    mov eax, ebx
    add eax, edx
    sub eax, 13          ; button_x = x + w - 13
    mov [wcb_x], eax

    mov eax, ecx
    add eax, 3           ; button_y = y + 3
    mov [wcb_y], eax

    ; Draw body (light gray)
    mov ebx, [wcb_x]
    mov ecx, [wcb_y]
    mov edx, 8
    mov esi, 8
    mov al, 7
    call draw_rect_pm

    ; Top highlight (white)
    mov ebx, [wcb_x]
    mov ecx, [wcb_y]
    mov edx, 8
    mov esi, 1
    mov al, 15
    call draw_rect_pm
    ; Left highlight (white)
    mov ebx, [wcb_x]
    mov ecx, [wcb_y]
    mov edx, 1
    mov esi, 8
    mov al, 15
    call draw_rect_pm

    ; Bottom shadow (dark gray)
    mov ebx, [wcb_x]
    mov ecx, [wcb_y]
    add ecx, 7
    mov edx, 8
    mov esi, 1
    mov al, 8
    call draw_rect_pm
    ; Right shadow (dark gray)
    mov ebx, [wcb_x]
    add ebx, 7
    mov ecx, [wcb_y]
    mov edx, 1
    mov esi, 8
    mov al, 8
    call draw_rect_pm

    ; Draw letter X (black text)
    mov ebx, [wcb_x]
    mov ecx, [wcb_y]
    mov al, 'X'
    mov ah, 0
    call draw_char

    popad
    ret

; ---- focus_window ----
; Activates the window, deactivates all others, and moves it to the top of Z-order
; Input: ESI = pointer to Window structure
focus_window:
    pushad

    ; 1. Deactivate all windows
    mov edi, window_files
    mov dword [edi + WIN_ACTIVE], 0
    mov edi, window_terminal
    mov dword [edi + WIN_ACTIVE], 0
    mov edi, window_settings
    mov dword [edi + WIN_ACTIVE], 0
    mov edi, window_welcome
    mov dword [edi + WIN_ACTIVE], 0

    ; 2. Activate ESI window
    mov dword [esi + WIN_ACTIVE], 1

    ; 3. Update window_order array
    xor ecx, ecx
.find_loop:
    cmp ecx, 4
    jae .done
    mov eax, [window_order + ecx*4]
    cmp eax, esi
    je .found
    inc ecx
    jmp .find_loop

.found:
    ; Shift remaining elements left
.shift_loop:
    mov edx, ecx
    inc edx
    cmp edx, 4
    jae .place_top
    mov eax, [window_order + edx*4]
    mov [window_order + ecx*4], eax
    mov ecx, edx
    jmp .shift_loop

.place_top:
    mov [window_order + 3*4], esi

.done:
    popad
    ret

; ---- Content Drawers ----

draw_welcome_content_func:
    pushad
    
    ; Content Area Box
    mov ebx, [window_welcome + WIN_X]
    add ebx, 4
    mov ecx, [window_welcome + WIN_Y]
    add ecx, 14
    mov edx, [window_welcome + WIN_W]
    sub edx, 8
    mov esi, [window_welcome + WIN_H]
    sub esi, 18
    mov al, 15          ; white
    call draw_rect_pm

    ; Dark gray separator line
    mov ebx, [window_welcome + WIN_X]
    add ebx, 4
    mov ecx, [window_welcome + WIN_Y]
    add ecx, 14
    mov edx, [window_welcome + WIN_W]
    sub edx, 8
    mov esi, 1
    mov al, 8
    call draw_rect_pm

    ; Text Lines
    mov esi, win_msg1
    mov ebx, [window_welcome + WIN_X]
    add ebx, 10
    mov ecx, [window_welcome + WIN_Y]
    add ecx, 22
    mov dl, 0
    call draw_text

    mov esi, win_msg2
    mov ebx, [window_welcome + WIN_X]
    add ebx, 10
    mov ecx, [window_welcome + WIN_Y]
    add ecx, 38
    mov dl, 0
    call draw_text

    mov esi, win_msg3
    mov ebx, [window_welcome + WIN_X]
    add ebx, 10
    mov ecx, [window_welcome + WIN_Y]
    add ecx, 54
    mov dl, 1           ; Blue text
    call draw_text

    popad
    ret

draw_files_content_func:
    pushad

    ; Content Area
    mov ebx, [window_files + WIN_X]
    add ebx, 4
    mov ecx, [window_files + WIN_Y]
    add ecx, 14
    mov edx, [window_files + WIN_W]
    sub edx, 8
    mov esi, [window_files + WIN_H]
    sub esi, 24
    mov al, 15          ; white
    call draw_rect_pm

    ; Separator
    mov ebx, [window_files + WIN_X]
    add ebx, 4
    mov ecx, [window_files + WIN_Y]
    add ecx, 14
    mov edx, [window_files + WIN_W]
    sub edx, 8
    mov esi, 1
    mov al, 8
    call draw_rect_pm

    ; File rows (blue dots + filenames)
    ; Row 1
    mov ebx, [window_files + WIN_X]
    add ebx, 10
    mov ecx, [window_files + WIN_Y]
    add ecx, 22
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file1_str
    mov ebx, [window_files + WIN_X]
    add ebx, 20
    mov ecx, [window_files + WIN_Y]
    add ecx, 21
    mov dl, 0
    call draw_text

    ; Row 2
    mov ebx, [window_files + WIN_X]
    add ebx, 10
    mov ecx, [window_files + WIN_Y]
    add ecx, 34
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file2_str
    mov ebx, [window_files + WIN_X]
    add ebx, 20
    mov ecx, [window_files + WIN_Y]
    add ecx, 33
    mov dl, 0
    call draw_text

    ; Row 3
    mov ebx, [window_files + WIN_X]
    add ebx, 10
    mov ecx, [window_files + WIN_Y]
    add ecx, 46
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file3_str
    mov ebx, [window_files + WIN_X]
    add ebx, 20
    mov ecx, [window_files + WIN_Y]
    add ecx, 45
    mov dl, 0
    call draw_text

    ; Row 4
    mov ebx, [window_files + WIN_X]
    add ebx, 10
    mov ecx, [window_files + WIN_Y]
    add ecx, 58
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file4_str
    mov ebx, [window_files + WIN_X]
    add ebx, 20
    mov ecx, [window_files + WIN_Y]
    add ecx, 57
    mov dl, 0
    call draw_text

    ; Status Bar
    mov ebx, [window_files + WIN_X]
    add ebx, 4
    mov ecx, [window_files + WIN_Y]
    add ecx, [window_files + WIN_H]
    sub ecx, 9
    mov edx, [window_files + WIN_W]
    sub edx, 8
    mov esi, 8
    mov al, 7           ; gray background
    call draw_rect_pm

    mov esi, files_status_str
    mov ebx, [window_files + WIN_X]
    add ebx, 8
    mov ecx, [window_files + WIN_Y]
    add ecx, [window_files + WIN_H]
    sub ecx, 9
    mov dl, 8           ; dark text
    call draw_text

    popad
    ret

draw_terminal_content_func:
    pushad

    ; Content Area (black)
    mov ebx, [window_terminal + WIN_X]
    add ebx, 4
    mov ecx, [window_terminal + WIN_Y]
    add ecx, 14
    mov edx, [window_terminal + WIN_W]
    sub edx, 8
    mov esi, [window_terminal + WIN_H]
    sub esi, 18
    mov al, 0           ; black
    call draw_rect_pm

    ; Line 1: header
    mov esi, term_static0
    mov ebx, [window_terminal + WIN_X]
    add ebx, 10
    mov ecx, [window_terminal + WIN_Y]
    add ecx, 22
    mov dl, 10          ; Light green
    call draw_text

    ; Line 2: separator info
    mov esi, term_static1
    mov ebx, [window_terminal + WIN_X]
    add ebx, 10
    mov ecx, [window_terminal + WIN_Y]
    add ecx, 34
    mov dl, 7           ; Gray
    call draw_text

    ; Line 3: tip
    mov esi, term_static2
    mov ebx, [window_terminal + WIN_X]
    add ebx, 10
    mov ecx, [window_terminal + WIN_Y]
    add ecx, 46
    mov dl, 7
    call draw_text

    ; Line 4: blank spacer (black)

    ; Line 5: static prompt
    mov esi, term_static3
    mov ebx, [window_terminal + WIN_X]
    add ebx, 10
    mov ecx, [window_terminal + WIN_Y]
    add ecx, 70
    mov dl, 10          ; Green prompt
    call draw_text

    popad
    ret

draw_settings_content_func:
    pushad

    ; Content Area (light gray)
    mov ebx, [window_settings + WIN_X]
    add ebx, 4
    mov ecx, [window_settings + WIN_Y]
    add ecx, 14
    mov edx, [window_settings + WIN_W]
    sub edx, 8
    mov esi, [window_settings + WIN_H]
    sub esi, 18
    mov al, 7           ; light gray body
    call draw_rect_pm

    ; Info Labels
    mov esi, settings_msg1
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 22
    mov dl, 0           ; black
    call draw_text

    mov esi, settings_msg2
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 36
    mov dl, 0           ; black
    call draw_text

    mov esi, settings_msg3
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 50
    mov dl, 0           ; black
    call draw_text

    ; Draw mock "Save" button background
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 66
    mov edx, 40
    mov esi, 12
    mov al, 7
    call draw_rect_pm

    ; Button highlight
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 66
    mov edx, 40
    mov esi, 1
    mov al, 15
    call draw_rect_pm
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 66
    mov edx, 1
    mov esi, 12
    mov al, 15
    call draw_rect_pm

    ; Button shadow
    mov ebx, [window_settings + WIN_X]
    add ebx, 10
    mov ecx, [window_settings + WIN_Y]
    add ecx, 77
    mov edx, 40
    mov esi, 1
    mov al, 8
    call draw_rect_pm
    mov ebx, [window_settings + WIN_X]
    add ebx, 49
    mov ecx, [window_settings + WIN_Y]
    add ecx, 66
    mov edx, 1
    mov esi, 12
    mov al, 8
    call draw_rect_pm

    ; Button text
    mov esi, settings_btn_str
    mov ebx, [window_settings + WIN_X]
    add ebx, 18
    mov ecx, [window_settings + WIN_Y]
    add ecx, 68
    mov dl, 0           ; black text
    call draw_text

    popad
    ret


; =============================================
; WINDOW MANAGER DATA
; =============================================

; ---- Dynamic Window Instances ----
window_files:
    dd 20           ; x
    dd 20           ; y
    dd 180          ; width
    dd 100          ; height
    dd files_win_title
    dd 0            ; visible
    dd 0            ; active
    dd 7            ; color (light gray)

window_terminal:
    dd 50           ; x
    dd 45           ; y
    dd 180          ; width
    dd 100          ; height
    dd terminal_win_title
    dd 0            ; visible
    dd 0            ; active
    dd 7            ; color

window_settings:
    dd 80           ; x
    dd 70           ; y
    dd 160          ; width
    dd 90           ; height
    dd settings_win_title
    dd 0            ; visible
    dd 0            ; active
    dd 7            ; color

window_welcome:
    dd 60           ; x
    dd 30           ; y
    dd 190          ; width
    dd 95           ; height
    dd welcome_win_title
    dd 1            ; visible (Welcome is open on boot)
    dd 1            ; active (Welcome is focused on boot)
    dd 7            ; color

; ---- Z-Order List (Bottom to Top) ----
window_order:
    dd window_files
    dd window_terminal
    dd window_settings
    dd window_welcome

; ---- Drag & Hit Test State Variables ----
dragged_window  dd 0
drag_offset_x   dd 0
drag_offset_y   dd 0
temp_index      dd 0
temp_index2     dd 0

wb_x            dd 0
wb_y            dd 0
wb_w            dd 0
wb_h            dd 0
wb_c            db 0

wt_x            dd 0
wt_y            dd 0
wt_w            dd 0
wt_title        dd 0
wt_active       db 0
wt_text_color   db 0

wcb_x           dd 0
wcb_y           dd 0

; ---- Title Strings ----
files_win_title     db 'FILES',0
terminal_win_title  db 'TERMINAL',0
settings_win_title  db 'SETTINGS',0
welcome_win_title   db 'WELCOME',0

; ---- Dialog Strings ----
win_msg1            db 'Welcome to SQ-OS!',0
win_msg2            db 'Architecture: 32-bit PM',0
win_msg3            db 'Status: GUI Active',0

files_status_str    db '4 items',0
file1_str           db 'kernel.asm',0
file2_str           db 'boot.asm',0
file3_str           db 'readme.md',0
file4_str           db 'system.cfg',0

; Terminal static display strings
term_static0        db 'SQ-OS Terminal v2.0',0
term_static1        db 'Arch: 32-bit Protected Mode',0
term_static2        db 'GUI Mode Active',0
term_static3        db 'SQ> _',0

settings_msg1       db 'Res: 320x200',0
settings_msg2       db 'Colors: 256',0
settings_msg3       db 'Mouse: OK',0
settings_btn_str    db 'Save',0
