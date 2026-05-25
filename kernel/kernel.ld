ENTRY(_start)

KERNEL_VMA = 0xffffffff80000000;

SECTIONS
{
    . = KERNEL_VMA;

    .text : ALIGN(4K) {
        KEEP(*(.multiboot2))
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
        *(.text .text.*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K) {
        *(.data .data.*)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss .bss.*)
    }
}
