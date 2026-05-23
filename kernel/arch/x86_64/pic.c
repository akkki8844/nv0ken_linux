#include "pic.h"

#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xa0
#define PIC2_DATA    0xa1
#define PIC_EOI      0x20

void pic_remap(uint8_t master_offset, uint8_t slave_offset)
{
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    outb(PIC1_DATA, master_offset);
    io_wait();
    outb(PIC2_DATA, slave_offset);
    io_wait();
    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, master_mask);
    outb(PIC2_DATA, slave_mask);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) {
        irq -= 8;
    }
    outb(port, inb(port) | (1u << irq));
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) {
        irq -= 8;
    }
    outb(port, inb(port) & ~(1u << irq));
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_disable(void)
{
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}
