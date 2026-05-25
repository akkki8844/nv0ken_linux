#include "icmp.h"

#include "../lib/string.h"
#include "checksum.h"

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t rest;
} icmp_header_t;

void icmp_receive(netdev_t *dev, const ipv4_header_t *ip, const void *payload, size_t length)
{
    if (!dev || !ip || !payload || length < sizeof(icmp_header_t)) {
        return;
    }

    const icmp_header_t *incoming = payload;
    if (incoming->type != 8 || incoming->code != 0) {
        return;
    }

    uint8_t reply[1500];
    if (length > sizeof(reply)) {
        return;
    }

    memcpy(reply, payload, length);
    icmp_header_t *header = (icmp_header_t *)reply;
    header->type = 0;
    header->checksum = 0;
    header->checksum = net_checksum(reply, length);
    ipv4_send(dev, ip->src, IPV4_PROTO_ICMP, reply, length);
}
