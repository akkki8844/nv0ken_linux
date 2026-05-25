#include "net_buf.h"

#include "../lib/string.h"
#include "../mm/heap.h"

net_buf_t *net_buf_alloc(size_t capacity)
{
    net_buf_t *buf = kcalloc(1, sizeof(net_buf_t));
    if (!buf) {
        return 0;
    }

    buf->data = kmalloc(capacity);
    if (!buf->data) {
        kfree(buf);
        return 0;
    }
    buf->capacity = capacity;
    return buf;
}

void net_buf_free(net_buf_t *buf)
{
    if (!buf) {
        return;
    }
    kfree(buf->data);
    kfree(buf);
}

int net_buf_append(net_buf_t *buf, const void *data, size_t length)
{
    if (!buf || (!data && length) || buf->length + length > buf->capacity) {
        return -1;
    }
    if (length) {
        memcpy(buf->data + buf->length, data, length);
    }
    buf->length += length;
    return 0;
}

int net_buf_prepend(net_buf_t *buf, const void *data, size_t length)
{
    if (!buf || (!data && length) || buf->length + length > buf->capacity) {
        return -1;
    }
    memmove(buf->data + length, buf->data, buf->length);
    if (length) {
        memcpy(buf->data, data, length);
    }
    buf->length += length;
    return 0;
}
