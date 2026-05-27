; =============================================
; gui/window.asm
; Window instances: SQ FILES, cursor
; To add a new app window: add draw_XXX_window + string data here
; =============================================

; ---- draw_cursor_pm ----
; Draws a 5x5 white square at current mouse position (more visible)
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

; ---- draw_files_window ----
; SQ FILES window: centered, X=30, Y=20, 260x140
; Layout:
;   Title bar: full width at top
;   Content: white area below title bar
;   4 file rows with colored icon dots + filename text
draw_files_window:
    pushad

    ; Draw window frame: (30, 20, 260, 140)
    mov ebx, 30
    mov ecx, 20
    mov edx, 260
    mov esi, 140
    mov edi, files_win_title
    call draw_window_pm

    ; White content area inside: (35, 37, 250, 118)
    mov ebx, 35
    mov ecx, 37
    mov edx, 250
    mov esi, 118
    mov al, 15
    call draw_rect_pm

    ; Thin separator line under title bar (dark gray)
    mov ebx, 35
    mov ecx, 37
    mov edx, 250
    mov esi, 1
    mov al, 8
    call draw_rect_pm

    ; ---- File row 1 ----
    ; Blue icon dot
    mov ebx, 42
    mov ecx, 48
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    ; Filename text
    mov esi, file1_str
    mov ebx, 54
    mov ecx, 49
    mov dl, 0
    call draw_text

    ; ---- File row 2 ----
    mov ebx, 42
    mov ecx, 66
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file2_str
    mov ebx, 54
    mov ecx, 67
    mov dl, 0
    call draw_text

    ; ---- File row 3 ----
    mov ebx, 42
    mov ecx, 84
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file3_str
    mov ebx, 54
    mov ecx, 85
    mov dl, 0
    call draw_text

    ; ---- File row 4 ----
    mov ebx, 42
    mov ecx, 102
    mov edx, 6
    mov esi, 6
    mov al, 1
    call draw_rect_pm
    mov esi, file4_str
    mov ebx, 54
    mov ecx, 103
    mov dl, 0
    call draw_text

    ; Status bar: dark gray strip at bottom of content
    mov ebx, 35
    mov ecx, 148
    mov edx, 250
    mov esi, 9
    mov al, 7
    call draw_rect_pm
    mov esi, files_status_str
    mov ebx, 40
    mov ecx, 150
    mov dl, 8           ; dark text
    call draw_text

    popad
    ret

; =============================================
; WINDOW DATA
; =============================================
files_open          db 0        ; 1 = SQ FILES window visible
desktop_win_open    db 1        ; 1 = Welcome window visible

win_welcome_title   db 'SQ-OS Welcome',0
win_msg1            db 'Welcome to SQ-OS!',0
win_msg2            db 'Architecture: 32-bit PM',0
win_msg3            db 'Status: GUI Active',0

files_win_title     db 'SQ Files',0
files_status_str    db '4 items',0
file1_str           db 'kernel.asm',0
file2_str           db 'boot.asm',0
file3_str           db 'readme.md',0
file4_str           db 'system.cfg',0
