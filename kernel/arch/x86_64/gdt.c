#include "gdt.h"

#include "lib/string.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

static gdt_entry_t gdt[5];
static gdt_ptr_t gdt_ptr;

static void set_entry(int index, uint32_t base, uint32_t limit,
                      uint8_t access, uint8_t flags)
{
    gdt[index].limit_low = (uint16_t)(limit & 0xffff);
    gdt[index].base_low = (uint16_t)(base & 0xffff);
    gdt[index].base_mid = (uint8_t)((base >> 16) & 0xff);
    gdt[index].access = access;
    gdt[index].granularity = (uint8_t)(((limit >> 16) & 0x0f) | (flags & 0xf0));
    gdt[index].base_high = (uint8_t)((base >> 24) & 0xff);
}

void gdt_init(void)
{
    memset(gdt, 0, sizeof(gdt));
    set_entry(1, 0, 0xfffff, 0x9a, 0xa0);
    set_entry(2, 0, 0xfffff, 0x92, 0xa0);
    set_entry(3, 0, 0xfffff, 0xf2, 0xa0);
    set_entry(4, 0, 0xfffff, 0xfa, 0xa0);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    gdt_load(&gdt_ptr);
    gdt_flush_segments();
}
