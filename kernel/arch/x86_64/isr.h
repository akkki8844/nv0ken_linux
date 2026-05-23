#ifndef NV0KEN_ARCH_X86_64_ISR_H
#define NV0KEN_ARCH_X86_64_ISR_H

#include <stdint.h>
#include "registers.h"

extern uint64_t isr_stub_table[256];

void isr_dispatch(registers_t *regs);
const char *isr_exception_name(uint64_t vector);

#endif
