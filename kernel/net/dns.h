#ifndef NV0KEN_NET_DNS_H
#define NV0KEN_NET_DNS_H

#include <stddef.h>
#include <stdint.h>

size_t dns_build_query(uint8_t *buffer, size_t capacity, uint16_t id, const char *hostname);

#endif
