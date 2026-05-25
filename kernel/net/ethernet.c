#include "ethernet.h"

#include "../lib/string.h"
#include "../mm/heap.h"
#include "arp.h"
#include "ipv4.h"

uint16_t net_htons(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

uint32_t net_htonl(uint32_t value)
{
    return ((value & 0x000000FFu) << 24) |
           ((value & 0x0000FF00u) << 8) |
           ((value & 0x00FF0000u) >> 8) |
           ((value & 0xFF000000u) >> 24);
}

int ethernet_send(netdev_t *dev, const uint8_t dst[6], uint16_t type,
                  const void *payload, size_t length)
{
    if (!dev || !dst || (!payload && length)) {
        return -1;
    }

    size_t frame_len = sizeof(ethernet_header_t) + length;
    uint8_t *frame = kmalloc(frame_len);
    if (!frame) {
        return -1;
    }

    ethernet_header_t *header = (ethernet_header_t *)frame;
    memcpy(header->dst, dst, 6);
    memcpy(header->src, dev->mac, 6);
    header->type = net_htons(type);
    if (length) {
        memcpy(frame + sizeof(*header), payload, length);
    }

    int status = netdev_send(dev, frame, frame_len);
    kfree(frame);
    return status;
}

void ethernet_receive(netdev_t *dev, const void *frame, size_t length)
{
    if (!dev || !frame || length < sizeof(ethernet_header_t)) {
        return;
    }

    const ethernet_header_t *header = frame;
    uint16_t type = net_htons(header->type);
    const uint8_t *payload = (const uint8_t *)frame + sizeof(*header);
    size_t payload_len = length - sizeof(*header);

    if (type == ETHERTYPE_ARP) {
        arp_receive(dev, payload, payload_len);
    } else if (type == ETHERTYPE_IPV4) {
        ipv4_receive(dev, payload, payload_len);
    }
}
