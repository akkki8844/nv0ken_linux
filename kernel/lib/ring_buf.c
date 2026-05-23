#include "ring_buf.h"

void ring_buf_init(ring_buf_t *ring, void *storage, size_t capacity)
{
    if (!ring) {
        return;
    }
    ring->data = storage;
    ring->capacity = capacity;
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

void ring_buf_reset(ring_buf_t *ring)
{
    if (!ring) {
        return;
    }
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

size_t ring_buf_capacity(const ring_buf_t *ring)
{
    return ring ? ring->capacity : 0;
}

size_t ring_buf_size(const ring_buf_t *ring)
{
    return ring ? ring->count : 0;
}

size_t ring_buf_space(const ring_buf_t *ring)
{
    return ring ? ring->capacity - ring->count : 0;
}

int ring_buf_empty(const ring_buf_t *ring)
{
    return !ring || ring->count == 0;
}

int ring_buf_full(const ring_buf_t *ring)
{
    return ring && ring->capacity > 0 && ring->count == ring->capacity;
}

int ring_buf_push(ring_buf_t *ring, uint8_t value)
{
    if (!ring || !ring->data || ring_buf_full(ring)) {
        return -1;
    }
    ring->data[ring->head] = value;
    ring->head = (ring->head + 1) % ring->capacity;
    ++ring->count;
    return 0;
}

int ring_buf_pop(ring_buf_t *ring, uint8_t *out)
{
    if (!ring || !ring->data || ring_buf_empty(ring)) {
        return -1;
    }
    if (out) {
        *out = ring->data[ring->tail];
    }
    ring->tail = (ring->tail + 1) % ring->capacity;
    --ring->count;
    return 0;
}

size_t ring_buf_write(ring_buf_t *ring, const void *data, size_t length)
{
    const uint8_t *bytes = data;
    size_t written = 0;
    while (written < length && ring_buf_push(ring, bytes[written]) == 0) {
        ++written;
    }
    return written;
}

size_t ring_buf_read(ring_buf_t *ring, void *data, size_t length)
{
    uint8_t *bytes = data;
    size_t read = 0;
    while (read < length && ring_buf_pop(ring, &bytes[read]) == 0) {
        ++read;
    }
    return read;
}
