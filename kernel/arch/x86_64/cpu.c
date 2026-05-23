#include "cpu.h"

void cpu_init(void)
{
    cpu_cli();
}

void cpu_get_vendor(char out[13])
{
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    __asm__ volatile("cpuid"
                     : "=b"(ebx), "=d"(edx), "=c"(ecx)
                     : "a"(0));
    ((uint32_t *)out)[0] = ebx;
    ((uint32_t *)out)[1] = edx;
    ((uint32_t *)out)[2] = ecx;
    out[12] = '\0';
}

uint64_t cpu_read_cr0(void)
{
    uint64_t value;
    __asm__ volatile("mov %%cr0, %0" : "=r"(value));
    return value;
}

uint64_t cpu_read_cr2(void)
{
    uint64_t value;
    __asm__ volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

uint64_t cpu_read_cr3(void)
{
    uint64_t value;
    __asm__ volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

uint64_t cpu_read_cr4(void)
{
    uint64_t value;
    __asm__ volatile("mov %%cr4, %0" : "=r"(value));
    return value;
}

void cpu_write_cr3(uint64_t value)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}
