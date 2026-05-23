#include "serial.h"

#include "arch/x86_64/io.h"

static uint16_t active_port;
static bool initialized;

bool serial_init(uint16_t port)
{
    active_port = port;

    outb(port + 1, 0x00);
    outb(port + 3, 0x80);
    outb(port + 0, 0x03);
    outb(port + 1, 0x00);
    outb(port + 3, 0x03);
    outb(port + 2, 0xc7);
    outb(port + 4, 0x0b);
    outb(port + 4, 0x1e);
    outb(port + 0, 0xae);

    if (inb(port + 0) != 0xae) {
        initialized = false;
        return false;
    }

    outb(port + 4, 0x0f);
    initialized = true;
    return true;
}

bool serial_ready(void)
{
    return initialized;
}

static bool transmit_empty(void)
{
    return (inb(active_port + 5) & 0x20) != 0;
}

void serial_write_char(char ch)
{
    if (!initialized) {
        return;
    }

    if (ch == '\n') {
        serial_write_char('\r');
    }

    while (!transmit_empty()) {
    }

    outb(active_port, (uint8_t)ch);
}

void serial_write(const char *text)
{
    if (!text) {
        return;
    }

    while (*text) {
        serial_write_char(*text++);
    }
}
