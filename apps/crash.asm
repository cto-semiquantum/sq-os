[bits 32]
;; =================================================================
;; crash.asm — Ring 3 privilege crash test for SQ-OS
;;
;; This app runs in Ring 3 (user mode). It intentionally writes to
;; address 0xC0000000, which is well beyond the 32MB physical RAM
;; mapped by the identity-paging setup and therefore NOT in any
;; valid page table entry.
;;
;; Expected behaviour:
;;   1. CPU raises #PF (Page Fault, INT 14)
;;   2. CPU sees CPL change (3 → 0) and switches to kernel stack via TSS
;;   3. page_fault_isr fires → calls page_fault_handler()
;;   4. Kernel prints "CRASH: CRASH Segfault" to the terminal
;;   5. Process is marked TERMINATED, scheduler picks next process
;;   6. Desktop / other apps continue running normally — kernel SURVIVES!
;;
;; Build: nasm -f bin apps/crash.asm -o crash.bin
;; =================================================================

start:
    ; Access unmapped high address → guaranteed Page Fault
    mov  eax, 0xC0000000
    mov  dword [eax], 0xDEADBEEF    ; #PF fires here

    ; Should never reach this point.
    ; Use INT 0x80 (DPL=3 gate, Ring 3 accessible) to exit cleanly.
    mov  eax, 5          ; SYS_EXIT syscall number
    xor  ebx, ebx        ; exit code 0
    int  0x80            ; Ring 3 syscall gateway
