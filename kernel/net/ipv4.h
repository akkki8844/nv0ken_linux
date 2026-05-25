#ifndef NV0KEN_NET_IPV4_H
#define NV0KEN_NET_IPV4_H

#include <stddef.h>
#include <stdint.h>

#include "netdev.h"

#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP 6
#define IPV4_PROTO_UDP 17

typedef struct __attribute__((packed)) {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} ipv4_header_t;

int ipv4_send(netdev_t *dev, uint32_t dst, uint8_t protocol, const void *payload, size_t length);
void ipv4_receive(netdev_t *dev, const void *packet, size_t length);

#endif
