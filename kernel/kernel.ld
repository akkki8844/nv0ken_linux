ENTRY(_start)

KERNEL_VMA = 0xffffffff80000000;

PHDRS
{
    boot PT_LOAD FLAGS(7);
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 1M;

    .boot : ALIGN(4K) {
        KEEP(*(.multiboot2))
        *(.boot.text)
        *(.boot.setup)
        *(.boot.data)
        *(.boot.bss)
    } :boot

    KERNEL_LMA = ALIGN(LOADADDR(.boot) + SIZEOF(.boot), 4K);
    . = KERNEL_VMA + KERNEL_LMA;

    .text ALIGN(4K) : AT(KERNEL_LMA) {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
        *(.text .text.*)
    } :text

    .rodata ALIGN(4K) : AT(ALIGN(LOADADDR(.text) + SIZEOF(.text), 4K)) {
        *(.rodata .rodata.*)
    } :rodata

    .data ALIGN(4K) : AT(ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4K)) {
        *(.data .data.*)
    } :data

    .bss ALIGN(4K) : AT(ALIGN(LOADADDR(.data) + SIZEOF(.data), 4K)) {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    __kernel_virtual_end = ALIGN(., 4K);

    /DISCARD/ : {
        *(.eh_frame*)
        *(.note*)
    }
}
