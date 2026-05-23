#ifndef NV0KEN_ARCH_X86_64_IDT_H
#define NV0KEN_ARCH_X86_64_IDT_H

#include <stdint.h>

#define IDT_ENTRIES 256
#define IDT_FLAG_INTERRUPT_GATE 0x8e
#define IDT_FLAG_USER_GATE      0xee

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t flags);
void idt_load(idt_ptr_t *ptr);

#endif
