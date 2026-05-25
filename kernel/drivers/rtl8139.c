#include "rtl8139.h"

#include "arch/x86_64/io.h"
#include "lib/string.h"

#define RTL_MAC0 0x00
#define RTL_TX_STATUS0 0x10
#define RTL_TX_ADDR0 0x20
#define RTL_COMMAND 0x37
#define RTL_CONFIG1 0x52

int rtl8139_init(rtl8139_t *dev, uint16_t io_base)
{
    if (!dev || io_base == 0) {
        return -1;
    }
    memset(dev, 0, sizeof(*dev));
    dev->io_base = io_base;
    outb(io_base + RTL_CONFIG1, 0x00);
    outb(io_base + RTL_COMMAND, 0x10);
    for (int i = 0; i < 6; ++i) {
        dev->mac[i] = inb((uint16_t)(io_base + RTL_MAC0 + i));
    }
    outb(io_base + RTL_COMMAND, 0x0c);
    dev->present = 1;
    return 0;
}

int rtl8139_send(rtl8139_t *dev, const void *data, size_t length)
{
    if (!dev || !dev->present || !data || length == 0 || length > 1792) {
        return -1;
    }
    outl((uint16_t)(dev->io_base + RTL_TX_ADDR0), (uint32_t)(uintptr_t)data);
    outl((uint16_t)(dev->io_base + RTL_TX_STATUS0), (uint32_t)length);
    return 0;
}

const uint8_t *rtl8139_mac(const rtl8139_t *dev)
{
    return dev ? dev->mac : 0;
}
