bits 64

section .text
global _start

extern main
extern exit

_start:
    xor rbp, rbp

    mov rdi, [rsp]
    lea rsi, [rsp + 8]
    lea rdx, [rsi + rdi*8 + 8]

    and rsp, ~15

    call main

    mov rdi, rax
    call exit

    ud2