bits 32

section .multiboot2
align 8
header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd -(0xe85250d6 + 0 + (header_end - header_start))

    align 8
    dw 3
    dw 0
    dd 12
    dd multiboot2_start

    align 8
    dw 0
    dw 0
    dd 8
header_end:

extern multiboot2_start
