#ifndef NV0KEN_LIB_RING_BUF_H
#define NV0KEN_LIB_RING_BUF_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} ring_buf_t;

void   ring_buf_init(ring_buf_t *ring, void *storage, size_t capacity);
void   ring_buf_reset(ring_buf_t *ring);
size_t ring_buf_capacity(const ring_buf_t *ring);
size_t ring_buf_size(const ring_buf_t *ring);
size_t ring_buf_space(const ring_buf_t *ring);
int    ring_buf_empty(const ring_buf_t *ring);
int    ring_buf_full(const ring_buf_t *ring);
int    ring_buf_push(ring_buf_t *ring, uint8_t value);
int    ring_buf_pop(ring_buf_t *ring, uint8_t *out);
size_t ring_buf_write(ring_buf_t *ring, const void *data, size_t length);
size_t ring_buf_read(ring_buf_t *ring, void *data, size_t length);

#endif
