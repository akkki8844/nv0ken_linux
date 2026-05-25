#ifndef NV0KEN_DRIVERS_RTL8139_H
#define NV0KEN_DRIVERS_RTL8139_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t io_base;
    uint8_t mac[6];
    int present;
} rtl8139_t;

int rtl8139_init(rtl8139_t *dev, uint16_t io_base);
int rtl8139_send(rtl8139_t *dev, const void *data, size_t length);
const uint8_t *rtl8139_mac(const rtl8139_t *dev);

#endif
