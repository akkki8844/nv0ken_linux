#ifndef NV0KEN_NET_ICMP_H
#define NV0KEN_NET_ICMP_H

#include <stddef.h>

#include "ipv4.h"

void icmp_receive(netdev_t *dev, const ipv4_header_t *ip, const void *payload, size_t length);

#endif
