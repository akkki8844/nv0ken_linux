global multiboot2_start
global _start
global multiboot2_magic
global multiboot2_info
extern kmain
extern multiboot2_boot_magic
extern multiboot2_boot_info

%define MULTIBOOT2_BOOT_MAGIC 0x36d76289
%define CR0_PAGING            (1 << 31)
%define CR4_PAE               (1 << 5)
%define EFER_MSR              0xc0000080
%define EFER_LONG_MODE        (1 << 8)

section .boot.text
bits 32
multiboot2_start:
    cli
    mov [multiboot2_magic], eax
    mov [multiboot2_info], ebx
    cmp eax, MULTIBOOT2_BOOT_MAGIC
    jne .boot_error

    mov esp, boot_stack_top
    mov ebp, esp

    mov eax, boot_pdpt
    or eax, 0x03
    mov [boot_pml4], eax
    mov [boot_pml4 + 511 * 8], eax

    xor ecx, ecx
.map_page_directory:
    mov eax, ecx
    shl eax, 12
    add eax, boot_page_directories
    or eax, 0x03
    mov [boot_pdpt + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .map_page_directory

    mov eax, boot_page_directories
    or eax, 0x03
    mov [boot_pdpt + 510 * 8], eax

    xor ecx, ecx
.map_large_page:
    mov eax, ecx
    shl eax, 21
    or eax, 0x83
    mov [boot_page_directories + ecx * 8], eax
    inc ecx
    cmp ecx, 2048
    jne .map_large_page

    mov eax, cr4
    or eax, CR4_PAE
    mov cr4, eax

    mov eax, boot_pml4
    mov cr3, eax

    mov ecx, EFER_MSR
    rdmsr
    or eax, EFER_LONG_MODE
    wrmsr

    mov eax, cr0
    or eax, CR0_PAGING
    mov cr0, eax

    lgdt [boot_gdt_pointer]
    jmp 0x08:long_mode_entry

.boot_error:
    mov word [0xb8000], 0x4f45
    mov word [0xb8002], 0x4f52
    mov word [0xb8004], 0x4f52
    cli
.boot_halt:
    hlt
    jmp .boot_halt

bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor eax, eax
    mov fs, ax
    mov gs, ax
    mov rax, multiboot2_boot_magic
    mov edx, [multiboot2_magic]
    mov [rax], rdx
    mov rax, multiboot2_boot_info
    mov edx, [multiboot2_info]
    mov [rax], rdx
    mov rax, _start
    jmp rax

section .boot.data
align 8
boot_gdt:
    dq 0
    dq 0x00af9a000000ffff
    dq 0x00af92000000ffff
boot_gdt_end:

boot_gdt_pointer:
    dw boot_gdt_end - boot_gdt - 1
    dd boot_gdt

align 8
multiboot2_magic:
    dq 0
multiboot2_info:
    dq 0

section .boot.bss nobits
align 4096
boot_pml4:
    resq 512
boot_pdpt:
    resq 512
boot_page_directories:
    resq 512 * 4

align 16
boot_stack_bottom:
    resb 16384
boot_stack_top:

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
