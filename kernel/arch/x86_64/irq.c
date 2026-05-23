#include "irq.h"

#include "pic.h"

#define IRQ_LINES 16

typedef struct {
    irq_handler_t handler;
    void *context;
} irq_slot_t;

static irq_slot_t irq_slots[IRQ_LINES];

void irq_init(void)
{
    pic_remap(PIC_REMAP_OFFSET, PIC_REMAP_OFFSET + 8);
    for (unsigned i = 0; i < IRQ_LINES; ++i) {
        pic_mask_irq((uint8_t)i);
        irq_slots[i].handler = 0;
        irq_slots[i].context = 0;
    }
}

int irq_register(unsigned irq, irq_handler_t handler, void *context)
{
    if (irq >= IRQ_LINES || !handler) {
        return -1;
    }
    irq_slots[irq].handler = handler;
    irq_slots[irq].context = context;
    pic_unmask_irq((uint8_t)irq);
    return 0;
}

void irq_unregister(unsigned irq)
{
    if (irq >= IRQ_LINES) {
        return;
    }
    pic_mask_irq((uint8_t)irq);
    irq_slots[irq].handler = 0;
    irq_slots[irq].context = 0;
}

void irq_dispatch(registers_t *regs)
{
    unsigned irq = (unsigned)(regs->int_no - PIC_REMAP_OFFSET);
    if (irq < IRQ_LINES && irq_slots[irq].handler) {
        irq_slots[irq].handler(regs, irq_slots[irq].context);
    }
    if (irq < IRQ_LINES) {
        pic_send_eoi((uint8_t)irq);
    }
}
