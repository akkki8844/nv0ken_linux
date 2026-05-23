#include "idt.h"

#include "gdt.h"
#include "isr.h"
#include "lib/string.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector, uint8_t flags)
{
    idt[vector].offset_low = (uint16_t)(handler & 0xffff);
    idt[vector].selector = selector;
    idt[vector].ist = 0;
    idt[vector].flags = flags;
    idt[vector].offset_mid = (uint16_t)((handler >> 16) & 0xffff);
    idt[vector].offset_high = (uint32_t)((handler >> 32) & 0xffffffff);
    idt[vector].reserved = 0;
}

void idt_init(void)
{
    memset(idt, 0, sizeof(idt));
    for (uint16_t vector = 0; vector < IDT_ENTRIES; ++vector) {
        idt_set_gate((uint8_t)vector, isr_stub_table[vector],
                     GDT_KERNEL_CODE, IDT_FLAG_INTERRUPT_GATE);
    }
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint64_t)&idt;
    idt_load(&idt_ptr);
}
