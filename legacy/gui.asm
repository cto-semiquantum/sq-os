; =============================================
; gui/gui.asm
; GUI subsystem entry, main event loop, and event handling
; =============================================

do_gui:
    cli
    ; === SET VGA MODE 13h via direct register writes ===
    ; Misc Output
    mov dx, 0x3C2
    mov al, 0x63
    out dx, al

    ; Sequencer
    mov dx, 0x3C4
    mov al, 0x00 ; index
    out dx, al
    inc dx
    mov al, 0x03 ; reset
    out dx, al
    dec dx
    mov al, 0x01
    out dx, al
    inc dx
    mov al, 0x01
    out dx, al
    dec dx
    mov al, 0x02
    out dx, al
    inc dx
    mov al, 0x0F
    out dx, al
    dec dx
    mov al, 0x03
    out dx, al
    inc dx
    mov al, 0x00
    out dx, al
    dec dx
    mov al, 0x04
    out dx, al
    inc dx
    mov al, 0x0E
    out dx, al

    ; Unlock CRTC
    mov dx, 0x3D4
    mov al, 0x11
    out dx, al
    inc dx
    mov al, 0x8E
    out dx, al
    dec dx

    ; CRTC registers
    mov esi, crtc13h
    mov ecx, 25
.crtc_loop:
    mov al, [esi]
    out dx, al
    inc dx
    inc esi
    mov al, [esi]
    out dx, al
    dec dx
    inc esi
    loop .crtc_loop

    ; Graphics Controller
    mov dx, 0x3CE
    mov esi, gc13h
    mov ecx, 9
.gc_loop:
    mov al, [esi]
    out dx, al
    inc dx
    inc esi
    mov al, [esi]
    out dx, al
    dec dx
    inc esi
    loop .gc_loop

    ; Attribute Controller — reset flip-flop first
    mov dx, 0x3DA
    in al, dx
    mov dx, 0x3C0
    mov esi, ac13h
    mov ecx, 21
.ac_loop:
    mov al, [esi]
    out dx, al
    inc esi
    mov al, [esi]
    out dx, al
    inc esi
    loop .ac_loop
    mov al, 0x20
    out dx, al

    ; === INITIALIZE PS/2 MOUSE ===
    ; Drain any stale data in the output buffer first
.ps2_drain:
    in al, 0x64
    test al, 0x01
    jz .ps2_drain_done
    in al, 0x60
    jmp .ps2_drain
.ps2_drain_done:

    ; Step 1: Enable auxiliary mouse device
    call ps2_wait_write
    mov al, 0xA8
    out 0x64, al

    ; Step 2: Read controller command byte
    call ps2_wait_write
    mov al, 0x20
    out 0x64, al
    ; Wait up to ~65535 iterations for data
    mov ecx, 65535
.wait_ccb:
    in al, 0x64
    test al, 0x01
    jnz .read_ccb
    loop .wait_ccb
    jmp .skip_ccb_mod   ; Timed out — skip modification
.read_ccb:
    in al, 0x60
    ; Enable keyboard interrupt (bit 0), enable mouse interrupt (bit 1)
    ; Keep keyboard clock enabled (bit 4 = 0), keep keyboard translation (bit 6 = 1)
    ; Keep mouse clock enabled (bit 5 = 0)
    or al, 0x02         ; enable IRQ12 for mouse
    and al, 0xDF        ; enable mouse clock (clear bit 5)
    push eax
    call ps2_wait_write
    mov al, 0x60
    out 0x64, al
    call ps2_wait_write
    pop eax
    out 0x60, al
.skip_ccb_mod:

    ; Step 3: Tell mouse to use default settings
    call ps2_wait_write
    mov al, 0xD4
    out 0x64, al
    call ps2_wait_write
    mov al, 0xF6        ; Set defaults
    out 0x60, al
    ; Drain ACK non-blockingly
    mov ecx, 65535
.drain_ack1:
    in al, 0x64
    test al, 0x01
    jnz .got_ack1
    loop .drain_ack1
    jmp .skip_ack1
.got_ack1:
    in al, 0x60
.skip_ack1:

    ; Step 4: Enable mouse packet streaming
    call ps2_wait_write
    mov al, 0xD4
    out 0x64, al
    call ps2_wait_write
    mov al, 0xF4
    out 0x60, al
    ; Drain ACK non-blockingly
    mov ecx, 65535
.drain_ack2:
    in al, 0x64
    test al, 0x01
    jnz .got_ack2
    loop .drain_ack2
    jmp .skip_ack2
.got_ack2:
    in al, 0x60
.skip_ack2:

    ; Reset GUI & Window variables
    mov dword [mouse_x], 160
    mov dword [mouse_y], 100
    mov byte [mouse_cycle], 0
    mov byte [last_mouse_buttons], 0
    mov dword [dragged_window], 0

    ; Reset all windows to default positions and visibility
    mov edi, window_files
    mov dword [edi + WIN_X], 20
    mov dword [edi + WIN_Y], 20
    mov dword [edi + WIN_VISIBLE], 0
    mov dword [edi + WIN_ACTIVE], 0

    mov edi, window_terminal
    mov dword [edi + WIN_X], 50
    mov dword [edi + WIN_Y], 45
    mov dword [edi + WIN_VISIBLE], 0
    mov dword [edi + WIN_ACTIVE], 0

    mov edi, window_settings
    mov dword [edi + WIN_X], 80
    mov dword [edi + WIN_Y], 70
    mov dword [edi + WIN_VISIBLE], 0
    mov dword [edi + WIN_ACTIVE], 0

    mov edi, window_welcome
    mov dword [edi + WIN_X], 60
    mov dword [edi + WIN_Y], 30
    mov dword [edi + WIN_VISIBLE], 1
    mov dword [edi + WIN_ACTIVE], 1

    ; Set initial Z-order (Welcome on top)
    mov dword [window_order + 0*4], window_files
    mov dword [window_order + 1*4], window_terminal
    mov dword [window_order + 2*4], window_settings
    mov dword [window_order + 3*4], window_welcome

    ; Initial redraw
    call redraw_desktop_pm

    ; === MAIN GUI LOOP ===
.gui_frame_loop:
    ; 1. Drain input queue
.input_loop:
    in al, 0x64
    test al, 0x01       ; Output buffer full?
    jz .render_frame    ; If no input, render frame

    test al, 0x20       ; Bit 5 = mouse data
    jnz .mouse_data

    ; Keyboard data
    in al, 0x60
    cmp al, 0x01        ; ESC key pressed?
    je .exit_gui
    jmp .input_loop

.mouse_data:
    in al, 0x60
    
    ; Process byte based on current packet cycle (0, 1, 2)
    movzx ecx, byte [mouse_cycle]
    cmp ecx, 0
    je .cycle0
    cmp ecx, 1
    je .cycle1
    cmp ecx, 2
    je .cycle2
    jmp .input_loop

.cycle0:
    test al, 0x08       ; sync bit check
    jz .input_loop      ; out of sync, drop byte
    mov [mouse_byte0], al
    inc byte [mouse_cycle]
    jmp .input_loop

.cycle1:
    mov [mouse_byte1], al
    inc byte [mouse_cycle]
    jmp .input_loop

.cycle2:
    mov [mouse_byte2], al
    mov byte [mouse_cycle], 0

    ; Full packet received!
    mov al, [mouse_byte0]
    mov bl, al          ; BL = buttons

    ; Process X movement
    movzx ecx, byte [mouse_byte1]
    test al, 0x10       ; sign X
    jz .x_pos
    or ecx, 0xFFFFFF00
.x_pos:
    add [mouse_x], ecx

    ; Process Y movement (Y axis is inverted relative to screen)
    movzx edx, byte [mouse_byte2]
    test al, 0x20       ; sign Y
    jz .y_pos
    or edx, 0xFFFFFF00
.y_pos:
    sub [mouse_y], edx

    ; Clamp X (0..319)
    cmp dword [mouse_x], 0
    jge .chk_x_max
    mov dword [mouse_x], 0
    jmp .clamp_y
.chk_x_max:
    cmp dword [mouse_x], 319
    jle .clamp_y
    mov dword [mouse_x], 319

.clamp_y:
    ; Clamp Y (0..199)
    cmp dword [mouse_y], 0
    jge .chk_y_max
    mov dword [mouse_y], 0
    jmp .handle_drag_click
.chk_y_max:
    cmp dword [mouse_y], 199
    jle .handle_drag_click
    mov dword [mouse_y], 199

.handle_drag_click:
    ; BL = current buttons, last_mouse_buttons = previous
    test bl, 1          ; Left button pressed?
    jnz .left_pressed

    ; Left button NOT pressed. If we were dragging a window, stop dragging.
    mov dword [dragged_window], 0
    jmp .update_last_buttons

.left_pressed:
    ; Left button is pressed. Was it pressed before?
    test byte [last_mouse_buttons], 1
    jnz .left_held

    ; Transition 0 -> 1: Fresh click!
    call handle_mouse_click
    jmp .update_last_buttons

.left_held:
    ; Left button is held down. If we are dragging a window, update its position.
    mov eax, [dragged_window]
    or eax, eax
    jz .update_last_buttons

    ; Calculate new window position
    mov ebx, [mouse_x]
    sub ebx, [drag_offset_x]
    mov ecx, [mouse_y]
    sub ecx, [drag_offset_y]

    ; Clamp so title bar stays on screen
    cmp ebx, -100
    jg .cx_min_ok
    mov ebx, -100
.cx_min_ok:
    cmp ebx, 280
    jl .cx_max_ok
    mov ebx, 280
.cx_max_ok:

    cmp ecx, 10
    jg .cy_min_ok
    mov ecx, 10
.cy_min_ok:
    cmp ecx, 180
    jl .cy_max_ok
    mov ecx, 180
.cy_max_ok:

    ; Update position of dragged window
    mov esi, [dragged_window]
    mov [esi + WIN_X], ebx
    mov [esi + WIN_Y], ecx

.update_last_buttons:
    mov [last_mouse_buttons], bl
    jmp .input_loop

.render_frame:
    ; Redraw everything to the backbuffer and copy to VGA screen
    call redraw_desktop_pm

    ; Wait a bit (approx 10-15ms for smooth stable framerate)
    mov ecx, 0x8000
.delay:
    dec ecx
    jnz .delay
    jmp .gui_frame_loop

.exit_gui:
    ; === RESTORE TEXT MODE 3 ===
    cli
    mov al, 0x67
    mov dx, 0x3C2
    out dx, al

    ; Sequencer
    mov dx, 0x3C4
    mov al, 0x00
    out dx, al
    inc dx
    mov al, 0x03
    out dx, al
    dec dx
    mov al, 0x01
    out dx, al
    inc dx
    mov al, 0x00
    out dx, al
    dec dx
    mov al, 0x02
    out dx, al
    inc dx
    mov al, 0x03
    out dx, al
    dec dx
    mov al, 0x03
    out dx, al
    inc dx
    mov al, 0x00
    out dx, al
    dec dx
    mov al, 0x04
    out dx, al
    inc dx
    mov al, 0x02
    out dx, al

    ; GC text mode
    mov dx, 0x3CE
    mov esi, gctext
    mov ecx, 9
.gc_text:
    mov al, [esi]
    out dx, al
    inc dx
    inc esi
    mov al, [esi]
    out dx, al
    dec dx
    inc esi
    loop .gc_text

    sti
    call clear_screen
    call draw_header
    mov dword [cur_row], 2
    mov dword [cur_col], 0
    jmp dispatch_done

; ---- handle_mouse_click ----
; Checks window bounds in reverse Z-order to process click events
handle_mouse_click:
    pushad
    mov ecx, [mouse_x]
    mov edx, [mouse_y]

    ; Start checking windows from top to bottom (index 3 to 0)
    mov dword [temp_index], 3
.win_loop:
    mov eax, [temp_index]
    cmp eax, 0
    jl .check_icons

    mov esi, [window_order + eax*4]

    ; Check if window is visible
    cmp dword [esi + WIN_VISIBLE], 1
    jne .next_win

    ; Check if mouse inside window boundary: [x .. x+w] and [y .. y+h]
    mov ebx, [esi + WIN_X]
    cmp ecx, ebx
    jl .next_win
    add ebx, [esi + WIN_W]
    cmp ecx, ebx
    jge .next_win

    mov ebx, [esi + WIN_Y]
    cmp edx, ebx
    jl .next_win
    add ebx, [esi + WIN_H]
    cmp edx, ebx
    jge .next_win

    ; Hit! Focus this window first
    call focus_window

    ; Check if close button is clicked:
    ; Close button X: [win_x + win_w - 13 .. win_x + win_w - 5]
    ; Close button Y: [win_y + 3 .. win_y + 11]
    mov ebx, [esi + WIN_X]
    add ebx, [esi + WIN_W]
    sub ebx, 13
    cmp ecx, ebx
    jl .check_titlebar
    add ebx, 8
    cmp ecx, ebx
    jge .check_titlebar

    mov ebx, [esi + WIN_Y]
    add ebx, 3
    cmp edx, ebx
    jl .check_titlebar
    add ebx, 8
    cmp edx, ebx
    jge .check_titlebar

    ; Close button hit!
    mov dword [esi + WIN_VISIBLE], 0
    mov dword [esi + WIN_ACTIVE], 0

    ; Focus the next visible window in Z-order
    mov dword [temp_index2], 3
.focus_next:
    mov eax, [temp_index2]
    cmp eax, 0
    jl .exit
    mov edi, [window_order + eax*4]
    cmp dword [edi + WIN_VISIBLE], 1
    je .found_next
    dec dword [temp_index2]
    jmp .focus_next
.found_next:
    mov esi, edi
    call focus_window
    jmp .exit

.check_titlebar:
    ; Check if titlebar is clicked:
    ; Titlebar X: [win_x .. win_x + win_w - 14]
    ; Titlebar Y: [win_y .. win_y + 12]
    mov ebx, [esi + WIN_X]
    cmp ecx, ebx
    jl .exit
    add ebx, [esi + WIN_W]
    sub ebx, 14
    cmp ecx, ebx
    jge .exit

    mov ebx, [esi + WIN_Y]
    cmp edx, ebx
    jl .exit
    add ebx, 12
    cmp edx, ebx
    jge .exit

    ; Titlebar hit! Start drag
    mov [dragged_window], esi
    mov eax, [mouse_x]
    sub eax, [esi + WIN_X]
    mov [drag_offset_x], eax
    mov eax, [mouse_y]
    sub eax, [esi + WIN_Y]
    mov [drag_offset_y], eax
    jmp .exit

.next_win:
    dec dword [temp_index]
    jmp .win_loop

.check_icons:
    ; Hit test desktop icons (SETTINGS, FILES, TERMINAL)
    ; SETTINGS Icon hitbox: X in 5..45, Y in 20..50
    cmp ecx, 5
    jl .check_files
    cmp ecx, 45
    jg .check_files
    cmp edx, 20
    jl .check_files
    cmp edx, 50
    jg .check_files

    ; Open and focus Settings
    mov dword [window_settings + WIN_VISIBLE], 1
    mov esi, window_settings
    call focus_window
    jmp .exit

.check_files:
    ; FILES Icon hitbox: X in 5..45, Y in 60..90
    cmp ecx, 5
    jl .check_term
    cmp ecx, 45
    jg .check_term
    cmp edx, 60
    jl .check_term
    cmp edx, 90
    jg .check_term

    ; Open and focus Files
    mov dword [window_files + WIN_VISIBLE], 1
    mov esi, window_files
    call focus_window
    jmp .exit

.check_term:
    ; TERMINAL Icon hitbox: X in 5..45, Y in 100..130
    cmp ecx, 5
    jl .exit
    cmp ecx, 45
    jg .exit
    cmp edx, 100
    jl .exit
    cmp edx, 130
    jg .exit

    ; Open and focus Terminal
    mov dword [window_terminal + WIN_VISIBLE], 1
    mov esi, window_terminal
    call focus_window

.exit:
    popad
    ret

; =============================================
; PS/2 CONTROLLER STATUS POLLING HELPERS
; =============================================

; ps2_wait_write: waits until the input buffer is empty (bit 1 of port 0x64 is 0)
ps2_wait_write:
    push eax
.loop:
    in al, 0x64
    test al, 0x02
    jnz .loop
    pop eax
    ret

; ps2_wait_read: waits until the output buffer is full (bit 0 of port 0x64 is 1)
ps2_wait_read:
    push eax
.loop:
    in al, 0x64
    test al, 0x01
    jz .loop
    pop eax
    ret

