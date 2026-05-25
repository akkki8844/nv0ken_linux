#include "ipv4.h"

#include "../lib/string.h"
#include "../mm/heap.h"
#include "arp.h"
#include "checksum.h"
#include "ethernet.h"
#include "icmp.h"
#include "tcp.h"
#include "udp.h"

static uint16_t next_id = 1;

int ipv4_send(netdev_t *dev, uint32_t dst, uint8_t protocol, const void *payload, size_t length)
{
    if (!dev || (!payload && length)) {
        return -1;
    }

    size_t packet_len = sizeof(ipv4_header_t) + length;
    uint8_t *packet = kmalloc(packet_len);
    if (!packet) {
        return -1;
    }

    ipv4_header_t *header = (ipv4_header_t *)packet;
    memset(header, 0, sizeof(*header));
    header->version_ihl = 0x45;
    header->total_length = net_htons((uint16_t)packet_len);
    header->identification = net_htons(next_id++);
    header->ttl = 64;
    header->protocol = protocol;
    header->src = dev->ipv4_addr;
    header->dst = dst;
    if (length) {
        memcpy(packet + sizeof(*header), payload, length);
    }
    header->checksum = net_checksum(header, sizeof(*header));

    uint8_t mac[6];
    int status = arp_lookup(dst, mac) ? ethernet_send(dev, mac, ETHERTYPE_IPV4, packet, packet_len) : -1;
    kfree(packet);
    return status;
}

void ipv4_receive(netdev_t *dev, const void *packet, size_t length)
{
    if (!dev || !packet || length < sizeof(ipv4_header_t)) {
        return;
    }

    const ipv4_header_t *header = packet;
    size_t ihl = (header->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(ipv4_header_t) || ihl > length) {
        return;
    }

    const void *payload = (const uint8_t *)packet + ihl;
    size_t payload_len = length - ihl;
    if (header->protocol == IPV4_PROTO_ICMP) {
        icmp_receive(dev, header, payload, payload_len);
    } else if (header->protocol == IPV4_PROTO_UDP) {
        udp_receive(dev, header, payload, payload_len);
    } else if (header->protocol == IPV4_PROTO_TCP) {
        tcp_receive(dev, header, payload, payload_len);
    }
}
