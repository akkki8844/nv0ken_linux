#include "checksum.h"

uint16_t net_checksum(const void *data, size_t length)
{
    const uint8_t *bytes = data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += ((uint16_t)bytes[0] << 8) | bytes[1];
        bytes += 2;
        length -= 2;
    }
    if (length) {
        sum += (uint16_t)bytes[0] << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

uint16_t net_checksum_pseudo_ipv4(uint32_t src, uint32_t dst, uint8_t proto,
                                  const void *data, size_t length)
{
    uint32_t sum = 0;
    sum += (src >> 16) & 0xFFFF;
    sum += src & 0xFFFF;
    sum += (dst >> 16) & 0xFFFF;
    sum += dst & 0xFFFF;
    sum += proto;
    sum += (uint16_t)length;

    const uint8_t *bytes = data;
    while (length > 1) {
        sum += ((uint16_t)bytes[0] << 8) | bytes[1];
        bytes += 2;
        length -= 2;
    }
    if (length) {
        sum += (uint16_t)bytes[0] << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}
