#ifndef NV0KEN_ARCH_X86_64_PIC_H
#define NV0KEN_ARCH_X86_64_PIC_H

#include <stdint.h>

#define PIC_REMAP_OFFSET 0x20

void pic_remap(uint8_t master_offset, uint8_t slave_offset);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);
void pic_send_eoi(uint8_t irq);
void pic_disable(void);

#endif
