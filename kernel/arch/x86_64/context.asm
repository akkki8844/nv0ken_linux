bits 64

global context_switch
global context_trampoline

section .text
context_switch:
    mov [rdi + 0x00], r15
    mov [rdi + 0x08], r14
    mov [rdi + 0x10], r13
    mov [rdi + 0x18], r12
    mov [rdi + 0x20], rbx
    mov [rdi + 0x28], rbp
    lea rax, [rel .resume]
    mov [rdi + 0x30], rax
    mov [rdi + 0x38], rsp
    pushfq
    pop qword [rdi + 0x40]

    mov r15, [rsi + 0x00]
    mov r14, [rsi + 0x08]
    mov r13, [rsi + 0x10]
    mov r12, [rsi + 0x18]
    mov rbx, [rsi + 0x20]
    mov rbp, [rsi + 0x28]
    mov rsp, [rsi + 0x38]
    push qword [rsi + 0x40]
    popfq
    jmp [rsi + 0x30]

.resume:
    ret

context_trampoline:
    pop rdi
    pop rax
    call rax
.halt:
    hlt
    jmp .halt
