#ifndef NV0KEN_ARCH_X86_64_GDT_H
#define NV0KEN_ARCH_X86_64_GDT_H

#include <stdint.h>

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

void gdt_init(void);
void gdt_load(gdt_ptr_t *ptr);
void gdt_flush_segments(void);

#endif
