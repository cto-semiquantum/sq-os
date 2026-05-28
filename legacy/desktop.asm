; =============================================
; gui/desktop.asm
; Full desktop frame pipeline: redraw_desktop_pm
; Draws background, topbar, taskbar, icons, windows, cursor
; Double-buffered: renders to 0x50000, then blits to 0xA0000
; =============================================

redraw_desktop_pm:
    pushad

    ; 1. Clear backbuffer (black)
    mov edi, BACKBUFFER
    mov ecx, 16000      ; 64000 bytes / 4 = 16000 dwords
    xor eax, eax
    cld
    rep stosd

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

    ; Icon: TERMINAL (light red block, X=15 Y=105)
    mov ebx, 15
    mov ecx, 105
    mov edx, 10
    mov esi, 10
    mov al, 12
    call draw_rect_pm
    mov esi, lbl_term
    mov ebx, 8
    mov ecx, 118
    mov dl, 15
    call draw_text

    ; 6. Draw all dynamic windows (in Z-order)
    call draw_all_windows

    ; 7. Cursor LAST (always on top)
    call draw_cursor_pm

    ; 8. Wait for vertical retrace (Vsync) to eliminate tearing/flicker
    mov dx, 0x3DA
.wait_retrace_end:
    in al, dx
    test al, 0x08
    jnz .wait_retrace_end
.wait_retrace_start:
    in al, dx
    test al, 0x08
    jz .wait_retrace_start

    ; Blit backbuffer -> VGA framebuffer
    mov esi, BACKBUFFER
    mov edi, VGA_FRAMEBUFFER
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
