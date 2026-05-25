#ifndef NV0KEN_NET_BUF_H
#define NV0KEN_NET_BUF_H

#include <stddef.h>
#include <stdint.h>

typedef struct net_buf {
    uint8_t *data;
    size_t length;
    size_t capacity;
    struct net_buf *next;
} net_buf_t;

net_buf_t *net_buf_alloc(size_t capacity);
void net_buf_free(net_buf_t *buf);
int net_buf_append(net_buf_t *buf, const void *data, size_t length);
int net_buf_prepend(net_buf_t *buf, const void *data, size_t length);

#endif
