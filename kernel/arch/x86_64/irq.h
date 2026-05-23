#ifndef NV0KEN_ARCH_X86_64_IRQ_H
#define NV0KEN_ARCH_X86_64_IRQ_H

#include "registers.h"

typedef void (*irq_handler_t)(registers_t *regs, void *context);

void irq_init(void);
int  irq_register(unsigned irq, irq_handler_t handler, void *context);
void irq_unregister(unsigned irq);
void irq_dispatch(registers_t *regs);

#endif
