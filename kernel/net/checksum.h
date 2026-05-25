#ifndef NV0KEN_NET_CHECKSUM_H
#define NV0KEN_NET_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

uint16_t net_checksum(const void *data, size_t length);
uint16_t net_checksum_pseudo_ipv4(uint32_t src, uint32_t dst, uint8_t proto,
                                  const void *data, size_t length);

#endif
