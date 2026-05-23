#include "context.h"

extern void context_trampoline(void);

void context_init(cpu_context_t *ctx, void *stack_top,
                  context_entry_t entry, void *arg)
{
    uint64_t *stack = (uint64_t *)stack_top;
    *--stack = (uint64_t)entry;
    *--stack = (uint64_t)arg;

    ctx->r15 = 0;
    ctx->r14 = 0;
    ctx->r13 = 0;
    ctx->r12 = 0;
    ctx->rbx = 0;
    ctx->rbp = 0;
    ctx->rip = (uint64_t)context_trampoline;
    ctx->rsp = (uint64_t)stack;
    ctx->rflags = 0x202;
}
