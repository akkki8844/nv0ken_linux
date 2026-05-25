#include "udp.h"

#include "../lib/string.h"
#include "../mm/heap.h"
#include "checksum.h"
#include "ethernet.h"

#define UDP_MAX_PORTS 16

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

typedef struct {
    uint16_t port;
    udp_handler_t handler;
} udp_binding_t;

static udp_binding_t bindings[UDP_MAX_PORTS];

void udp_bind(uint16_t port, udp_handler_t handler)
{
    for (size_t index = 0; index < UDP_MAX_PORTS; ++index) {
        if (bindings[index].port == 0 || bindings[index].port == port) {
            bindings[index].port = port;
            bindings[index].handler = handler;
            return;
        }
    }
}

int udp_send(netdev_t *dev, uint32_t dst, uint16_t src_port, uint16_t dst_port,
             const void *payload, size_t length)
{
    if (!dev || (!payload && length)) {
        return -1;
    }

    size_t datagram_len = sizeof(udp_header_t) + length;
    uint8_t *datagram = kmalloc(datagram_len);
    if (!datagram) {
        return -1;
    }

    udp_header_t *header = (udp_header_t *)datagram;
    header->src_port = net_htons(src_port);
    header->dst_port = net_htons(dst_port);
    header->length = net_htons((uint16_t)datagram_len);
    header->checksum = 0;
    if (length) {
        memcpy(datagram + sizeof(*header), payload, length);
    }
    header->checksum = net_checksum_pseudo_ipv4(dev->ipv4_addr, dst, IPV4_PROTO_UDP, datagram, datagram_len);
    int status = ipv4_send(dev, dst, IPV4_PROTO_UDP, datagram, datagram_len);
    kfree(datagram);
    return status;
}

void udp_receive(netdev_t *dev, const ipv4_header_t *ip, const void *payload, size_t length)
{
    if (!dev || !ip || !payload || length < sizeof(udp_header_t)) {
        return;
    }

    const udp_header_t *header = payload;
    uint16_t dst_port = net_htons(header->dst_port);
    const void *data = (const uint8_t *)payload + sizeof(*header);
    size_t data_len = length - sizeof(*header);

    for (size_t index = 0; index < UDP_MAX_PORTS; ++index) {
        if (bindings[index].port == dst_port && bindings[index].handler) {
            bindings[index].handler(dev, ip->src, net_htons(header->src_port), data, data_len);
        }
    }
}
