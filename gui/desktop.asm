; =============================================
; gui/desktop.asm
; Full desktop frame pipeline: redraw_desktop_pm
; Draws background, topbar, taskbar, icons, windows, cursor
; Double-buffered: renders to 0x50000, then blits to 0xA0000
; =============================================

redraw_desktop_pm:
    pushad

    ; Redirect all drawing to backbuffer
    mov dword [draw_buffer], 0x50000

    ; 1. Clear backbuffer (black)
    mov edi, 0x50000
    mov ecx, 64000
    xor al, al
    cld
    rep stosb

    ; 2. Desktop background (blue, Y=10..189)
    mov ebx, 0
    mov ecx, 10
    mov edx, 320
    mov esi, 180
    mov al, 1
    call draw_rect_pm

    ; 3. Top bar (cyan, Y=0..9)
    mov ebx, 0
    mov ecx, 0
    mov edx, 320
    mov esi, 10
    mov al, 3
    call draw_rect_pm

    mov esi, gui_title
    mov ebx, 100
    mov ecx, 1
    mov dl, 15
    call draw_text

    ; 4. Taskbar (dark gray, Y=190..199)
    mov ebx, 0
    mov ecx, 190
    mov edx, 320
    mov esi, 10
    mov al, 8
    call draw_rect_pm

    mov esi, lbl_esc
    mov ebx, 200
    mov ecx, 191
    mov dl, 15
    call draw_text

    ; 5. Desktop icons
    ; Icon: SETTINGS (yellow block, X=15 Y=25)
    mov ebx, 15
    mov ecx, 25
    mov edx, 10
    mov esi, 10
    mov al, 14
    call draw_rect_pm
    mov esi, lbl_set
    mov ebx, 8
    mov ecx, 38
    mov dl, 15
    call draw_text

    ; Icon: FILES (green block, X=15 Y=65)
    mov ebx, 15
    mov ecx, 65
    mov edx, 10
    mov esi, 10
    mov al, 10
    call draw_rect_pm
    mov esi, lbl_files
    mov ebx, 8
    mov ecx, 78
    mov dl, 15
    call draw_text

    ; 6. Welcome window (if open) — top area, smaller
    cmp byte [desktop_win_open], 1
    jne .skip_welcome
    mov ebx, 80
    mov ecx, 20
    mov edx, 190
    mov esi, 95
    mov edi, win_welcome_title
    call draw_window_pm
    ; Content area
    mov ebx, 85
    mov ecx, 37
    mov edx, 180
    mov esi, 73
    mov al, 15
    call draw_rect_pm
    ; Text
    mov esi, win_msg1
    mov ebx, 90
    mov ecx, 45
    mov dl, 0
    call draw_text
    mov esi, win_msg2
    mov ebx, 90
    mov ecx, 58
    mov dl, 0
    call draw_text
    mov esi, win_msg3
    mov ebx, 90
    mov ecx, 71
    mov dl, 1
    call draw_text
.skip_welcome:

    ; 7. Files window (if open)
    cmp byte [files_open], 1
    jne .skip_files
    call draw_files_window
.skip_files:

    ; 8. Cursor LAST (always on top)
    call draw_cursor_pm

    ; 9. Blit backbuffer -> VGA framebuffer
    mov esi, 0x50000
    mov edi, 0xA0000
    mov ecx, 16000
    cld
    rep movsd

    popad
    ret

; =============================================
; DESKTOP STRING DATA
; =============================================
gui_title   db 'SQ-OS DESKTOP v2.0',0
lbl_files   db 'FILES',0
lbl_term    db 'TERMINAL',0
lbl_set     db 'SETTINGS',0
lbl_esc     db 'ESC=SHELL',0
