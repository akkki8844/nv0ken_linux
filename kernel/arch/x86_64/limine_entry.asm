global _start

extern kmain

section .text
bits 64
_start:
    mov rsp, stack_top
    and rsp, -16
    call kmain

.halt:
    hlt
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
