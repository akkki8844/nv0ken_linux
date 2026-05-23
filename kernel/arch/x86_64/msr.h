#ifndef NV0KEN_ARCH_X86_64_MSR_H
#define NV0KEN_ARCH_X86_64_MSR_H

#include <stdint.h>

#define MSR_EFER        0xc0000080u
#define MSR_STAR        0xc0000081u
#define MSR_LSTAR       0xc0000082u
#define MSR_CSTAR       0xc0000083u
#define MSR_SFMASK      0xc0000084u
#define MSR_KERNEL_GS   0xc0000102u

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo;
    uint32_t hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

#endif
