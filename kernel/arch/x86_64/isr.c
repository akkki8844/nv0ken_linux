#include "isr.h"

#include "irq.h"
#include "pic.h"
#include "lib/kprintf.h"
#include "lib/panic.h"

static const char *exception_names[] = {
    "divide error",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "bound range exceeded",
    "invalid opcode",
    "device not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid tss",
    "segment not present",
    "stack segment fault",
    "general protection fault",
    "page fault",
    "reserved",
    "x87 floating-point exception",
    "alignment check",
    "machine check",
    "simd floating-point exception",
    "virtualization exception",
    "control protection exception",
};

const char *isr_exception_name(uint64_t vector)
{
    if (vector < sizeof(exception_names) / sizeof(exception_names[0])) {
        return exception_names[vector];
    }
    return "unknown exception";
}

void isr_dispatch(registers_t *regs)
{
    if (!regs) {
        panic("interrupt without register frame");
    }

    if (regs->int_no >= PIC_REMAP_OFFSET && regs->int_no < PIC_REMAP_OFFSET + 16) {
        irq_dispatch(regs);
        return;
    }

    if (regs->int_no < 32) {
        kprintf("exception %u: %s err=%x rip=%p\n",
                (unsigned)regs->int_no,
                isr_exception_name(regs->int_no),
                (unsigned)regs->err_code,
                (void *)regs->rip);
        panic("fatal cpu exception");
    }
}
