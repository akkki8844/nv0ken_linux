#include <stdint.h>

#define LAPIC_REG_EOI 0x0b0
#define LAPIC_REG_SVR 0x0f0

static volatile uint32_t *lapic_base;

void apic_init(uint64_t base)
{
    lapic_base = (volatile uint32_t *)(uintptr_t)base;
    if (lapic_base) {
        lapic_base[LAPIC_REG_SVR / 4] = lapic_base[LAPIC_REG_SVR / 4] | 0x100;
    }
}

void apic_send_eoi(void)
{
    if (lapic_base) {
        lapic_base[LAPIC_REG_EOI / 4] = 0;
    }
}

int apic_available(void)
{
    return lapic_base != 0;
}
