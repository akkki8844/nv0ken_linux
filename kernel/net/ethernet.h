#ifndef NV0KEN_NET_ETHERNET_H
#define NV0KEN_NET_ETHERNET_H

#include <stddef.h>
#include <stdint.h>

#include "netdev.h"

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP 0x0806

typedef struct __attribute__((packed)) {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} ethernet_header_t;

uint16_t net_htons(uint16_t value);
uint32_t net_htonl(uint32_t value);
int ethernet_send(netdev_t *dev, const uint8_t dst[6], uint16_t type,
                  const void *payload, size_t length);
void ethernet_receive(netdev_t *dev, const void *frame, size_t length);

#endif
