bits 64

extern syscall_dispatch
extern syscall_kernel_stack_top
global syscall_entry

section .text
syscall_entry:
    mov [rel syscall_user_stack], rsp
    mov rsp, [rel syscall_kernel_stack_top]
    push rcx
    push r11
    mov r11, r9
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    push qword 0
    push r11
    call syscall_dispatch
    add rsp, 16
    pop r11
    pop rcx
    mov rsp, [rel syscall_user_stack]
    sysretq

section .bss
align 8
syscall_user_stack:
    resq 1
