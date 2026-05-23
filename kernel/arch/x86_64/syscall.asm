bits 64

extern syscall_dispatch
global syscall_entry

section .text
syscall_entry:
    swapgs
    push rcx
    push r11
    mov r11, r9
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    push r11
    call syscall_dispatch
    add rsp, 8
    pop r11
    pop rcx
    swapgs
    sysretq
