#include "dns.h"

#include "../lib/string.h"

size_t dns_build_query(uint8_t *buffer, size_t capacity, uint16_t id, const char *hostname)
{
    if (!buffer || !hostname || capacity < 18) {
        return 0;
    }

    memset(buffer, 0, capacity);
    buffer[0] = (uint8_t)(id >> 8);
    buffer[1] = (uint8_t)id;
    buffer[2] = 1;
    buffer[5] = 1;

    size_t out = 12;
    const char *label = hostname;
    while (*label) {
        const char *dot = label;
        while (*dot && *dot != '.') {
            ++dot;
        }

        size_t len = (size_t)(dot - label);
        if (len > 63 || out + len + 6 > capacity) {
            return 0;
        }
        buffer[out++] = (uint8_t)len;
        memcpy(buffer + out, label, len);
        out += len;
        if (!*dot) {
            break;
        }
        label = dot + 1;
    }

    buffer[out++] = 0;
    buffer[out++] = 0;
    buffer[out++] = 1;
    buffer[out++] = 0;
    buffer[out++] = 1;
    return out;
}
