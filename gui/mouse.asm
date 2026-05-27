; =============================================
; gui/mouse.asm
; PS/2 mouse state variables
; =============================================

mouse_x             dd 160      ; current cursor X (0..319)
mouse_y             dd 100      ; current cursor Y (0..199)
mouse_cycle         db 0        ; packet byte index (0,1,2)
mouse_byte0         db 0        ; packet: buttons + sign bits
mouse_byte1         db 0        ; packet: delta X
mouse_byte2         db 0        ; packet: delta Y
last_mouse_buttons  db 0        ; previous button state for edge detection
