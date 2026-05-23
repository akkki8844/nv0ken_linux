#ifndef NV0KEN_ARCH_X86_64_CPU_H
#define NV0KEN_ARCH_X86_64_CPU_H

#include <stdint.h>

static inline void cpu_halt(void)
{
    __asm__ volatile("hlt");
}

static inline void cpu_pause(void)
{
    __asm__ volatile("pause");
}

static inline void cpu_cli(void)
{
    __asm__ volatile("cli" ::: "memory");
}

static inline void cpu_sti(void)
{
    __asm__ volatile("sti" ::: "memory");
}

static inline uint64_t cpu_read_rflags(void)
{
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    return flags;
}

static inline int cpu_interrupts_enabled(void)
{
    return (cpu_read_rflags() & (1ull << 9)) != 0;
}

void cpu_init(void);
void cpu_get_vendor(char out[13]);
uint64_t cpu_read_cr0(void);
uint64_t cpu_read_cr2(void);
uint64_t cpu_read_cr3(void);
uint64_t cpu_read_cr4(void);
void cpu_write_cr3(uint64_t value);

#endif
