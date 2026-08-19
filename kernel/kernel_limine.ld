ENTRY(_start)

KERNEL_VMA = 0xffffffff80000000;

PHDRS
{
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_VMA + 1M;

    .limine_requests_start ALIGN(8) : AT(1M) {
        KEEP(*(.limine_requests_start))
    } :text

    .limine_requests ALIGN(8) : AT(ALIGN(LOADADDR(.limine_requests_start) + SIZEOF(.limine_requests_start), 8)) {
        KEEP(*(.limine_requests))
    } :text

    .limine_requests_end ALIGN(8) : AT(ALIGN(LOADADDR(.limine_requests) + SIZEOF(.limine_requests), 8)) {
        KEEP(*(.limine_requests_end))
    } :text

    .text ALIGN(4K) : AT(ALIGN(LOADADDR(.limine_requests_end) + SIZEOF(.limine_requests_end), 4K)) {
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
