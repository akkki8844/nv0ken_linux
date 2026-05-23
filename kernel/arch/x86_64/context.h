#ifndef NV0KEN_ARCH_X86_64_CONTEXT_H
#define NV0KEN_ARCH_X86_64_CONTEXT_H

#include <stdint.h>

typedef struct cpu_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
} cpu_context_t;

typedef void (*context_entry_t)(void *arg);

void context_init(cpu_context_t *ctx, void *stack_top,
                  context_entry_t entry, void *arg);
void context_switch(cpu_context_t *current, cpu_context_t *next);

#endif
