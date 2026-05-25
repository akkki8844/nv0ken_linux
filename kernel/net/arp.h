#ifndef NV0KEN_NET_ARP_H
#define NV0KEN_NET_ARP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "netdev.h"

void arp_init(void);
void arp_learn(uint32_t ipv4, const uint8_t mac[6]);
bool arp_lookup(uint32_t ipv4, uint8_t mac[6]);
int arp_request(netdev_t *dev, uint32_t target_ipv4);
void arp_receive(netdev_t *dev, const void *packet, size_t length);

#endif
