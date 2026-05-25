#ifndef NV0KEN_NET_UDP_H
#define NV0KEN_NET_UDP_H

#include <stddef.h>
#include <stdint.h>

#include "ipv4.h"

typedef void (*udp_handler_t)(netdev_t *dev, uint32_t src, uint16_t src_port,
                              const void *payload, size_t length);

int udp_send(netdev_t *dev, uint32_t dst, uint16_t src_port, uint16_t dst_port,
             const void *payload, size_t length);
void udp_receive(netdev_t *dev, const ipv4_header_t *ip, const void *payload, size_t length);
void udp_bind(uint16_t port, udp_handler_t handler);

#endif
