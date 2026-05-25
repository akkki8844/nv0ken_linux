#include "arp.h"

#include "../lib/string.h"
#include "ethernet.h"

#define ARP_TABLE_SIZE 32

typedef struct {
    uint32_t ipv4;
    uint8_t mac[6];
    bool valid;
} arp_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} arp_packet_t;

static arp_entry_t table[ARP_TABLE_SIZE];

void arp_init(void)
{
    memset(table, 0, sizeof(table));
}

void arp_learn(uint32_t ipv4, const uint8_t mac[6])
{
    if (!mac) {
        return;
    }

    size_t slot = 0;
    for (size_t index = 0; index < ARP_TABLE_SIZE; ++index) {
        if (table[index].valid && table[index].ipv4 == ipv4) {
            slot = index;
            goto store;
        }
        if (!table[index].valid) {
            slot = index;
            break;
        }
    }

store:
    table[slot].ipv4 = ipv4;
    memcpy(table[slot].mac, mac, 6);
    table[slot].valid = true;
}

bool arp_lookup(uint32_t ipv4, uint8_t mac[6])
{
    if (!mac) {
        return false;
    }

    for (size_t index = 0; index < ARP_TABLE_SIZE; ++index) {
        if (table[index].valid && table[index].ipv4 == ipv4) {
            memcpy(mac, table[index].mac, 6);
            return true;
        }
    }
    return false;
}

int arp_request(netdev_t *dev, uint32_t target_ipv4)
{
    if (!dev) {
        return -1;
    }

    static const uint8_t broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    arp_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    packet.htype = net_htons(1);
    packet.ptype = net_htons(ETHERTYPE_IPV4);
    packet.hlen = 6;
    packet.plen = 4;
    packet.oper = net_htons(1);
    memcpy(packet.sha, dev->mac, 6);
    packet.spa = dev->ipv4_addr;
    packet.tpa = target_ipv4;
    return ethernet_send(dev, broadcast, ETHERTYPE_ARP, &packet, sizeof(packet));
}

void arp_receive(netdev_t *dev, const void *packet_data, size_t length)
{
    (void)dev;
    if (!packet_data || length < sizeof(arp_packet_t)) {
        return;
    }

    const arp_packet_t *packet = packet_data;
    arp_learn(packet->spa, packet->sha);
}
