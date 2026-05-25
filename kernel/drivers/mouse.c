#include "mouse.h"

#include "arch/x86_64/io.h"
#include "arch/x86_64/irq.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

static uint8_t packet[3];
static unsigned packet_index;
static mouse_event_fn event_handler;
static void *event_context;

static int wait_input_clear(void)
{
    for (unsigned spin = 0; spin < 100000; ++spin) {
        if ((inb(PS2_STATUS) & 2) == 0) {
            return 0;
        }
    }
    return -1;
}

static int mouse_write(uint8_t value)
{
    if (wait_input_clear() != 0) {
        return -1;
    }
    outb(PS2_CMD, 0xd4);
    if (wait_input_clear() != 0) {
        return -1;
    }
    outb(PS2_DATA, value);
    return 0;
}

static void mouse_irq(registers_t *regs, void *context)
{
    (void)regs;
    (void)context;
    uint8_t value = inb(PS2_DATA);
    if (packet_index == 0 && (value & 0x08) == 0) {
        return;
    }
    packet[packet_index++] = value;
    if (packet_index < 3) {
        return;
    }
    packet_index = 0;
    mouse_packet_t parsed = {
        .dx = (int8_t)packet[1],
        .dy = -(int8_t)packet[2],
        .buttons = packet[0] & 0x07,
    };
    if (event_handler) {
        event_handler(&parsed, event_context);
    }
}

void mouse_init(void)
{
    if (wait_input_clear() != 0) {
        return;
    }
    outb(PS2_CMD, 0xa8);
    if (mouse_write(0xf6) != 0) {
        return;
    }
    (void)inb(PS2_DATA);
    if (mouse_write(0xf4) != 0) {
        return;
    }
    (void)inb(PS2_DATA);
    irq_register(12, mouse_irq, 0);
}

void mouse_set_handler(mouse_event_fn handler, void *context)
{
    event_handler = handler;
    event_context = context;
}
