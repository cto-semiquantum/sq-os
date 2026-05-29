; =====================================================================
; INTERACTIVE SHELL MODULE
; Handles Commands parsing, Dispatching, and command executions
; =====================================================================

shell_start:
    mov di, [cursor]
    mov si, prompt
    call print
    call get_input

    mov bx, handlers
    mov di, commands
.next_cmd:
    cmp byte [di], 0
    je .not_found
    mov si, input
    call strcmp
    jc .match

    ; Advance di to the next command string
.skip_to_next:
    cmp byte [di], 0
    je .found_null
    inc di
    jmp .skip_to_next
.found_null:
    inc di              ; Move past the null terminator
    add bx, 2           ; Next handler
    jmp .next_cmd
.match:
    jmp word [bx]

.not_found:
    call newline_cursor
    jmp shell_start


; =====================================================================
; COMMAND HANDLERS
; =====================================================================

showhelp:
    call newline_cursor
    mov di, [cursor]
    mov si, commands
.loop:
    lodsb
    or al, al
    jz .space
    mov ah, [textcolor]
    stosw
    jmp .loop
.space:
    mov ax, 0x0b20      ; Space character
    mov ah, [textcolor]
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

showneofetch:
    mov si, neofetchmsg
    jmp print_msg_command

showbanner:
    mov si, bannermsg
    jmp print_msg_command

showdir:
    mov si, dirmsg
    jmp print_msg_command

showcd:
    mov si, cdmsg
    jmp print_msg_command

showcat:
    mov si, catmsg
    jmp print_msg_command

doclrg: 
    mov byte [textcolor], 0x0A      ; Green text
    jmp doclr_end
doclrb: 
    mov byte [textcolor], 0x09      ; Blue text
    jmp doclr_end
doclrr: 
    mov byte [textcolor], 0x0C      ; Red text
    jmp doclr_end
doclrrst: 
    mov byte [textcolor], 0x0F      ; White text
    jmp doclr_end
doclr_end:
    call newline_cursor
    jmp shell_start

doclear:
    call clear
    call draw_ui
    mov word [cursor], 1280
    jmp shell_start

docls:
    call clear
    mov word [cursor], 160
    jmp shell_start

doreboot:
    int 0x19

doshutdown:
    mov ax, 0x5307
    mov bx, 0x0001
    mov cx, 0x0003
    int 0x15

dobeep:
    in al, 0x61
    or al, 3
    out 0x61, al
    mov al, 182
    out 0x43, al
    mov ax, 2000
    out 0x42, al
    mov al, ah
    out 0x42, al
    mov cx, 0x0003
    mov dx, 0x0D40
    mov ah, 0x86
    int 0x15
    in al, 0x61
    and al, 0xFC
    out 0x61, al
    jmp print_msg_command_end

showdate:
    call newline_cursor
    mov di, [cursor]
    mov ah, 04h
    int 1ah
    mov al, dl
    call print_bcd
    mov ax, 0x0f2f
    mov ah, [textcolor]
    stosw
    mov al, dh
    call print_bcd
    mov ax, 0x0f2f
    mov ah, [textcolor]
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
    mov ah, [textcolor]
    stosw
    mov al, cl
    call print_bcd
    mov ax, 0x0f3a
    mov ah, [textcolor]
    stosw
    mov al, dl
    call print_bcd
    mov [cursor], di
    jmp print_msg_command_end

showmem:
    call newline_cursor
    mov di, [cursor]
    mov si, memmsg
    call print
    int 0x12
    mov di, num_buffer
    call itoa
    mov si, num_buffer
    call print
    mov si, kbmsg
    call print
    jmp print_msg_command_end

showcpu:
    call newline_cursor
    mov di, [cursor]
    mov si, cpumsg
    call print
    mov eax, 0
    cpuid
    mov dword [vendor_str], ebx
    mov dword [vendor_str+4], edx
    mov dword [vendor_str+8], ecx
    mov byte [vendor_str+12], 0
    mov si, vendor_str
    call print
    jmp print_msg_command_end

; Interactive mouse position checking
showmouse:
    mov ax, 0x0000
    int 0x33
    mov ax, 0x0001
    int 0x33            ; Show mouse pointer

    call newline_cursor
    mov di, [cursor]
    mov si, mouseinitmsg
    call print
    call newline_cursor

.mouseloop:
    mov ah, 0x01
    int 0x16            ; Check key status (non-blocking)
    jz .readmouse
    
    mov ah, 0x00
    int 0x16            ; Read key
    cmp ah, 0x01        ; ESC key pressed?
    je .mousedone

.readmouse:
    mov ax, 0x0003
    int 0x33            ; Read mouse position (BX=buttons, CX=X, DX=Y)
    
    ; Print on fixed line
    push cx
    push dx
    mov ah, 0x02
    mov bh, 0
    mov dh, 14
    mov dl, 0
    int 0x10

    mov si, mouseXmsg
.px:
    lodsb
    or al, al
    jz .px_done
    mov ah, 0x0E
    int 0x10
    jmp .px
.px_done:

    pop dx
    pop cx
    push cx
    push dx
    mov ax, cx
    call print_mouse_num

    mov si, mouseYmsg
.py:
    lodsb
    or al, al
    jz .py_done
    mov ah, 0x0E
    int 0x10
    jmp .py
.py_done:

    pop dx
    pop cx
    mov ax, dx
    call print_mouse_num

    ; Small delay
    mov ah, 0x86
    mov cx, 0
    mov dx, 0x8000
    int 0x15
    jmp .mouseloop

.mousedone:
    mov ax, 0x0002
    int 0x33            ; Hide mouse
    call newline_cursor
    jmp shell_start

; =====================================================================
; REDRAW DESKTOP GRAPHICS LAYOUT
; Redraws desktop based on open window state flags
; =====================================================================
redraw_desktop:
    pusha

    ; 1. Clear Screen to Black (color 0)
    mov al, 0
    call clear_screen

    ; 2. Draw Desktop Blue background (Y=10 to 189)
    mov cx, 0
    mov dx, 10
    mov si, 320
    mov di, 180
    mov al, 1           ; Dark blue desktop background
    call draw_rect

    ; 3. Draw Header Bar (Cyan)
    mov cx, 0
    mov dx, 0
    mov si, 320
    mov di, 10
    mov al, 3           ; Cyan top bar
    call draw_rect

    ; Draw GUI title in header cyan bar
    mov cx, 100
    mov dx, 1
    mov si, gui_title
    mov al, 0           ; Black text on cyan bar
    call draw_text

    ; 4. Draw Bottom Taskbar
    call draw_taskbar

    ; Draw Shell escape instruction on taskbar
    mov cx, 200
    mov dx, 191
    mov si, esc_txt
    mov al, 15          ; White text
    call draw_text

    ; 5. Draw Desktop Icons (using draw_icon!)
    ; Icon 0: Computer
    mov cx, 15
    mov dx, 25
    mov al, 0           ; Computer icon type
    mov bp, lbl_sys     ; Label string
    call draw_icon

    ; Icon 1: Files
    mov cx, 15
    mov dx, 65
    mov al, 1           ; Folder icon type
    mov bp, lbl_files   ; Label string
    call draw_icon

    ; 6. Draw Welcome Window if open
    cmp byte [desktop_win_open], 1
    jne .skip_desktop_win

    mov cx, [win_welcome_x]
    mov dx, [win_welcome_y]
    mov si, 190         ; Width
    mov di, 115         ; Height
    mov bp, win_title   ; Title string
    call draw_window

    ; Draw Window Content Area
    mov cx, [win_welcome_x]
    add cx, 5
    mov dx, [win_welcome_y]
    add dx, 15
    mov si, 180         ; Width
    mov di, 95          ; Height
    mov al, 15          ; White body background
    call draw_rect

    ; Render Text inside Window
    mov cx, [win_welcome_x]
    add cx, 10
    mov dx, [win_welcome_y]
    add dx, 23
    mov si, win_msg1
    mov al, 0           ; Black text
    call draw_text

    mov cx, [win_welcome_x]
    add cx, 10
    mov dx, [win_welcome_y]
    add dx, 35
    mov si, win_msg2
    mov al, 0           ; Black text
    call draw_text

    mov cx, [win_welcome_x]
    add cx, 10
    mov dx, [win_welcome_y]
    add dx, 47
    mov si, win_msg3
    mov al, 12          ; Red status text
    call draw_text
.skip_desktop_win:

    ; 7. Draw File Explorer Window if open
    cmp byte [explorer_win_open], 1
    jne .skip_explorer_win

    mov cx, [win_explorer_x]
    mov dx, [win_explorer_y]
    mov si, 160         ; Width
    mov di, 90          ; Height
    mov bp, exp_title   ; Title string pointer
    call draw_window

    ; Draw Window Content Area
    mov cx, [win_explorer_x]
    add cx, 5
    mov dx, [win_explorer_y]
    add dx, 15
    mov si, 150         ; Width
    mov di, 70          ; Height
    mov al, 15          ; White body background
    call draw_rect

    ; Render folder listing inside File Explorer window
    mov cx, [win_explorer_x]
    add cx, 10
    mov dx, [win_explorer_y]
    add dx, 22
    mov si, file1_txt
    mov al, 0           ; Black text
    call draw_text

    mov cx, [win_explorer_x]
    add cx, 10
    mov dx, [win_explorer_y]
    add dx, 34
    mov si, file2_txt
    mov al, 0           ; Black text
    call draw_text

    mov cx, [win_explorer_x]
    add cx, 10
    mov dx, [win_explorer_y]
    add dx, 46
    mov si, file3_txt
    mov al, 0           ; Black text
    call draw_text

    mov cx, [win_explorer_x]
    add cx, 10
    mov dx, [win_explorer_y]
    add dx, 58
    mov si, file4_txt
    mov al, 0           ; Black text
    call draw_text

    mov cx, [win_explorer_x]
    add cx, 10
    mov dx, [win_explorer_y]
    add dx, 70
    mov si, file5_txt
    mov al, 0           ; Black text
    call draw_text
.skip_explorer_win:

    ; 7b. Draw Start Menu if open
    cmp byte [start_menu_open], 1
    jne .skip_start_menu

    mov cx, 4           ; X
    mov dx, 120         ; Y
    mov si, 80          ; Width
    mov di, 70          ; Height
    mov bp, start_menu_title
    call draw_window

    ; Draw content background (white)
    mov cx, 8
    mov dx, 133
    mov si, 72
    mov di, 54
    mov al, 15
    call draw_rect

    ; Draw items
    mov cx, 12
    mov dx, 137
    mov si, menu_about_txt
    mov al, 0
    call draw_text

    mov cx, 12
    mov dx, 149
    mov si, menu_shell_txt
    mov al, 0
    call draw_text

    mov cx, 12
    mov dx, 161
    mov si, menu_reboot_txt
    mov al, 0
    call draw_text

    mov cx, 12
    mov dx, 173
    mov si, menu_shut_txt
    mov al, 0
    call draw_text
.skip_start_menu:

    ; 8. Draw mouse cursor at current coordinates LAST
    mov cx, [mouse_x]
    mov dx, [mouse_y]
    call draw_mouse_cursor

    popa
    ret

; -------------------------------------------------------------
; NEW Mode 13h VGA Desktop GUI implementation using Graphics Subsystem
; -------------------------------------------------------------
showgui:
    ; 1. Enter Mode 13h (320x200x256 Graphics)
    mov ax, 0x0013
    int 0x10

    ; Set ES to VGA memory segment
    mov ax, 0xA000
    mov es, ax

    ; 2. Reset visibility, position, and redraw states on start
    mov byte [desktop_win_open], 1
    mov byte [explorer_win_open], 0
    mov byte [start_menu_open], 0
    mov byte [drag_active], 0
    mov byte [last_mouse_buttons], 0

    mov word [win_welcome_x], 65
    mov word [win_welcome_y], 35
    mov word [win_explorer_x], 100
    mov word [win_explorer_y], 60

    ; Initialize coordinates in memory & request initial redraw
    mov word [mouse_x], 160
    mov word [mouse_y], 100
    mov byte [needs_redraw], 1

    ; 3. Initialize PS/2 Mouse Driver via BIOS
    mov ax, 0x0000
    int 0x33            ; returns AX=0xFFFF if mouse supported

    ; Hide default BIOS hardware pointer (so we draw our own graphical cursor)
    mov ax, 0x0002
    int 0x33

    ; Set mouse start position (coordinates in BIOS are 0..639 for X, so 320 maps to X=160)
    mov ax, 0x0004
    mov cx, 320         ; X = 160 graphical
    mov dx, 100         ; Y = 100 graphical
    int 0x33

    ; 4. Real-time Mouse Polling & State Update Loop
.gui_loop:
    ; Check for key press (ESC exits)
    mov ah, 0x01
    int 0x16
    jz .poll_mouse

    mov ah, 0x00
    int 0x16
    cmp al, 27          ; ESC key
    je .exit_gui

.poll_mouse:
    ; Read mouse position & button states
    mov ax, 0x0003
    int 0x33            ; CX = X (0-639), DX = Y (0-199), BX = buttons
    
    ; Scale X from 0-639 back to 0-319
    shr cx, 1

    ; Clamp X (cx) to 0..319
    cmp cx, 319
    jbe .x_ok
    cmp cx, 32768
    jae .x_zero
    mov cx, 319
    jmp .x_ok
.x_zero:
    mov cx, 0
.x_ok:

    ; Clamp Y (dx) to 0..199
    cmp dx, 199
    jbe .y_ok
    cmp dx, 32768
    jae .y_zero
    mov dx, 199
    jmp .y_ok
.y_zero:
    mov dx, 0
.y_ok:

    ; Save coordinates in memory
    mov [mouse_x], cx
    mov [mouse_y], dx

    ; Call redraw_desktop UNCONDITIONALLY on every frame/iteration
    call redraw_desktop
    jmp .gui_loop_delay

    ; Get current left click state
    mov al, bl
    and al, 1           ; AL = current left button state

    ; Get last left click state
    mov ah, [last_mouse_buttons]
    and ah, 1           ; AH = last left button state

    ; Update last_mouse_buttons in memory
    mov [last_mouse_buttons], bl

    ; Check if drag is active
    cmp byte [drag_active], 0
    je .no_drag_active

    ; A drag is active! Check if button is still pressed
    cmp al, 1
    je .do_drag

    ; Button was released! End drag
    mov byte [drag_active], 0
    jmp .gui_loop

.do_drag:
    cmp byte [drag_active], 1
    je .drag_welcome
    cmp byte [drag_active], 2
    je .drag_explorer
    jmp .gui_loop

.drag_welcome:
    mov ax, cx
    sub ax, [drag_offset_x]
    ; Clamp welcome window X to 0..130 (320 - 190 = 130)
    cmp ax, 32768
    jae .clamp_w_x0
    cmp ax, 130
    jle .w_x_ok
    mov ax, 130
    jmp .w_x_ok
.clamp_w_x0:
    xor ax, ax
.w_x_ok:
    mov [win_welcome_x], ax

    mov ax, dx
    sub ax, [drag_offset_y]
    ; Clamp welcome window Y to 10..75 (190 - 115 = 75)
    cmp ax, 32768
    jae .clamp_w_y10
    cmp ax, 10
    jl .clamp_w_y10
    cmp ax, 75
    jle .w_y_ok
    mov ax, 75
    jmp .w_y_ok
.clamp_w_y10:
    mov ax, 10
.w_y_ok:
    mov [win_welcome_y], ax
    jmp .gui_loop

.drag_explorer:
    mov ax, cx
    sub ax, [drag_offset_x]
    ; Clamp explorer window X to 0..160 (320 - 160 = 160)
    cmp ax, 32768
    jae .clamp_e_x0
    cmp ax, 160
    jle .e_x_ok
    mov ax, 160
    jmp .e_x_ok
.clamp_e_x0:
    xor ax, ax
.e_x_ok:
    mov [win_explorer_x], ax

    mov ax, dx
    sub ax, [drag_offset_y]
    ; Clamp explorer window Y to 10..100 (190 - 90 = 100)
    cmp ax, 32768
    jae .clamp_e_y10
    cmp ax, 10
    jl .clamp_e_y10
    cmp ax, 100
    jle .e_y_ok
    mov ax, 100
    jmp .e_y_ok
.clamp_e_y10:
    mov ax, 10
.e_y_ok:
    mov [win_explorer_y], ax
    jmp .gui_loop

.no_drag_active:
    ; No drag active. Check for left click transition (0 -> 1)
    cmp al, 1
    jne .gui_loop_delay
    cmp ah, 0
    jne .gui_loop_delay

    ; Left Click transition!
    
    ; 1. Check if Start Button was clicked (X: 4..40, Y: 191..199)
    cmp cx, 4
    jl .not_start_btn
    cmp cx, 40
    jg .not_start_btn
    cmp dx, 191
    jl .not_start_btn
    cmp dx, 199
    jg .not_start_btn
    
    ; Clicked start button! Toggle start menu state
    xor byte [start_menu_open], 1
    jmp .gui_loop_delay

.not_start_btn:
    ; 2. If Start Menu is open, check if clicked inside or outside of it
    cmp byte [start_menu_open], 1
    jne .check_windows

    ; Start Menu is open. Clicked outside Start Button.
    ; Is the click inside the Start Menu bounds? (X: 4..84, Y: 120..190)
    cmp cx, 4
    jl .close_menu
    cmp cx, 84
    jg .close_menu
    cmp dx, 120
    jl .close_menu
    cmp dx, 190
    jg .close_menu

    ; Clicked INSIDE Start Menu. Check item hitboxes:
    ; Item 1: ABOUT (X: 8..76, Y: 135..145)
    cmp dx, 135
    jl .chk_item2
    cmp dx, 145
    jg .chk_item2
    ; Clicked ABOUT! Open Welcome Window, close menu
    mov byte [desktop_win_open], 1
    mov byte [start_menu_open], 0
    jmp .gui_loop_delay

.chk_item2:
    ; Item 2: SHELL (X: 8..76, Y: 147..157)
    cmp dx, 147
    jl .chk_item3
    cmp dx, 157
    jg .chk_item3
    ; Clicked SHELL! Exit GUI
    mov byte [start_menu_open], 0
    jmp .exit_gui

.chk_item3:
    ; Item 3: REBOOT (X: 8..76, Y: 159..169)
    cmp dx, 159
    jl .chk_item4
    cmp dx, 169
    jg .chk_item4
    ; Clicked REBOOT! Switch to text mode first
    mov ax, 0x0003
    int 0x10
    jmp doreboot

.chk_item4:
    ; Item 4: SHUTDOWN (X: 8..76, Y: 171..181)
    cmp dx, 171
    jl .gui_loop_delay
    cmp dx, 181
    jg .gui_loop_delay
    ; Clicked SHUTDOWN! Switch to text mode first
    mov ax, 0x0003
    int 0x10
    jmp doshutdown

.close_menu:
    mov byte [start_menu_open], 0
    ; Fall through to check other clicks!

.check_windows:
    ; 3. Check Welcome Window Close Button & Title Bar if open
    cmp byte [desktop_win_open], 1
    jne .check_explorer_window

    ; Welcome close button
    mov ax, [win_welcome_x]
    add ax, 179
    cmp cx, ax
    jl .not_welcome_close
    add ax, 6
    cmp cx, ax
    jg .not_welcome_close
    mov ax, [win_welcome_y]
    add ax, 3
    cmp dx, ax
    jl .not_welcome_close
    add ax, 6
    cmp dx, ax
    jg .not_welcome_close
    ; Clicked close!
    mov byte [desktop_win_open], 0
    jmp .gui_loop_delay

.not_welcome_close:
    ; Welcome Title Bar Drag
    mov ax, [win_welcome_x]
    add ax, 2
    cmp cx, ax
    jl .not_welcome_title
    add ax, 186
    cmp cx, ax
    jg .not_welcome_title
    mov ax, [win_welcome_y]
    add ax, 2
    cmp dx, ax
    jl .not_welcome_title
    add ax, 10
    cmp dx, ax
    jg .not_welcome_title
    ; Yes, start drag welcome!
    mov byte [drag_active], 1
    sub cx, [win_welcome_x]
    mov [drag_offset_x], cx
    sub dx, [win_welcome_y]
    mov [drag_offset_y], dx
    jmp .gui_loop_delay
.not_welcome_title:

.check_explorer_window:
    ; 4. Check Explorer Window Close Button & Title Bar if open
    cmp byte [explorer_win_open], 1
    jne .check_icons

    ; Explorer close button
    mov ax, [win_explorer_x]
    add ax, 149
    cmp cx, ax
    jl .not_explorer_close
    add ax, 6
    cmp cx, ax
    jg .not_explorer_close
    mov ax, [win_explorer_y]
    add ax, 3
    cmp dx, ax
    jl .not_explorer_close
    add ax, 6
    cmp dx, ax
    jg .not_explorer_close
    ; Clicked close!
    mov byte [explorer_win_open], 0
    jmp .gui_loop_delay

.not_explorer_close:
    ; Explorer Title Bar Drag
    mov ax, [win_explorer_x]
    add ax, 2
    cmp cx, ax
    jl .not_explorer_title
    add ax, 156
    cmp cx, ax
    jg .not_explorer_title
    mov ax, [win_explorer_y]
    add ax, 2
    cmp dx, ax
    jl .not_explorer_title
    add ax, 10
    cmp dx, ax
    jg .not_explorer_title
    ; Yes, start drag explorer!
    mov byte [drag_active], 2
    sub cx, [win_explorer_x]
    mov [drag_offset_x], cx
    sub dx, [win_explorer_y]
    mov [drag_offset_y], dx
    jmp .gui_loop_delay
.not_explorer_title:

.check_icons:
    ; 5. Desktop Icons
    ; Files Icon clicked (X: 10..80, Y: 60..100)
    cmp cx, 10
    jl .check_sys_icon
    cmp cx, 80
    jg .check_sys_icon
    cmp dx, 60
    jl .check_sys_icon
    cmp dx, 100
    jg .check_sys_icon
    mov byte [explorer_win_open], 1
    jmp .gui_loop_delay

.check_sys_icon:
    ; System Icon clicked (X: 10..35, Y: 20..50)
    cmp cx, 10
    jl .gui_loop_delay
    cmp cx, 35
    jg .gui_loop_delay
    cmp dx, 20
    jl .gui_loop_delay
    cmp dx, 50
    jg .gui_loop_delay
    mov byte [desktop_win_open], 1

.gui_loop_delay:
    ; Small delay (approx 4ms) to prevent polling overhead
    mov ah, 0x86
    mov cx, 0
    mov dx, 0x1000
    int 0x15
    jmp .gui_loop

.exit_gui:
    ; Restore Text Mode
    mov ax, 0x0003
    int 0x10
    mov ax, 0xb800
    mov es, ax

    call clear
    call draw_ui
    mov word [cursor], 1280
    jmp shell_start


; =====================================================================
; HELPER SHELL PRINT FUNCTIONS
; =====================================================================

print_msg_command:
    call newline_cursor
    mov di, [cursor]
    call print
print_msg_command_end:
    call newline_cursor
    jmp shell_start


; =====================================================================
; SHELL DATA & STRINGS
; =====================================================================

commands db 'help',0,'clear',0,'about',0,'reboot',0,'shutdown',0,'version',0,'whoami',0,'date',0,'time',0,'echo',0,'cls',0,'calc',0,'uname',0,'exit',0,'neofetch',0,'color green',0,'color blue',0,'color red',0,'color reset',0,'beep',0,'banner',0,'dir',0,'cd',0,'cat',0,'mem',0,'cpu',0,'gui',0,'mouse',0,0
handlers dw showhelp, doclear, showversion, doreboot, doshutdown, showversion, showwhoami, showdate, showtime, showecho, docls, showcalc, showuname, showexit, showneofetch, doclrg, doclrb, doclrr, doclrrst, dobeep, showbanner, showdir, showcd, showcat, showmem, showcpu, showgui, showmouse

; Shell strings
prompt       db '> ',0
versionmsg   db 'SQ OS v1.0',0
whoamimsg    db 'harsh-cto',0
echomsg      db 'Echo working',0
calcmsg      db '2+2=4',0
unamemsg     db 'SQOS x86',0
exitmsg      db 'Cannot exit kernel shell',0
logs         db 'System Ready',0

neofetchmsg  db '   ___     OS: SQ OS',10
             db '  / _ \    Kernel: v1.0',10
             db ' | | | |   Arch: x86',10
             db ' | |_| |   Shell: SQSH',10
             db '  \___/ ',0

bannermsg    db '  ____   ___     ___  ____  ',10
             db ' / ___| / _ \   / _ \/ ___| ',10
             db ' \___ \| | | | | | | \___ \ ',10
             db '  ___) | |_| | | |_| |___) |',10
             db ' |____/ \__\_\  \___/|____/ ',0

dirmsg       db 'KERNEL.BIN   BOOT.BIN   CONFIG.SYS',0
cdmsg        db 'Access Denied: Root Directory Locked',0
catmsg       db 'Usage: cat [filename]',0
memmsg       db 'Base Memory: ',0
kbmsg        db ' KB',0
cpumsg       db 'Processor: ',0
num_buffer   db 0,0,0,0,0,0,0,0
vendor_str   db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

mouseinitmsg db 'Mouse active. Move cursor. Press ESC to exit.',0
mouseXmsg    db 'X: ',0
mouseYmsg    db '  Y: ',0

; GUI-specific strings
win_title    db 'SQ-OS Desktop',0
win_msg1     db 'Welcome to SemiQuantum!',0
win_msg2     db 'Architecture: 16-bit RM',0
win_msg3     db 'Status: Clean & Modular',0
lbl_files    db 'Files',0
lbl_sys      db 'System',0
start_txt    db 'START',0
esc_txt      db 'ESC: Shell',0
gui_title    db 'SemiQuantum OS GUI',0

; Explorer App strings
exp_title    db 'SQ FILES',0
file1_txt    db 'kernel.asm',0
file2_txt    db 'boot.asm',0
file3_txt    db 'notes.txt',0
file4_txt    db 'system.cfg',0
file5_txt    db 'readme.md',0
