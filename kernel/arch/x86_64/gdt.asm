bits 64

global gdt_load
global gdt_flush_segments

section .text
gdt_load:
    lgdt [rdi]
    ret

gdt_flush_segments:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    push qword 0x08
    lea rax, [rel .reload_cs]
    push rax
    retfq
.reload_cs:
    ret
