global _start

extern kmain

section .text
bits 64
_start:
    ; Limine has already supplied the requested stack.  Do not replace it
    ; with a smaller static stack: early memory initialisation can need the
    ; full requested stack before the heap is available.
    xor rbp, rbp
    and rsp, -16
    call kmain

.halt:
    hlt
    jmp .halt
