#include "pipe.h"

#include "../mm/heap.h"

int pipe_init(ipc_pipe_t *pipe, size_t capacity)
{
    if (!pipe) {
        return -1;
    }

    if (capacity == 0) {
        capacity = IPC_PIPE_DEFAULT_CAPACITY;
    }

    pipe->buffer = kmalloc(capacity);
    if (!pipe->buffer) {
        return -1;
    }

    pipe->capacity = capacity;
    pipe->read_index = 0;
    pipe->write_index = 0;
    pipe->size = 0;
    pipe->reader_closed = false;
    pipe->writer_closed = false;
    mutex_init(&pipe->lock);
    return 0;
}

void pipe_destroy(ipc_pipe_t *pipe)
{
    if (!pipe) {
        return;
    }

    kfree(pipe->buffer);
    pipe->buffer = 0;
    pipe->capacity = 0;
    pipe->size = 0;
}

size_t pipe_read(ipc_pipe_t *pipe, void *buffer, size_t count)
{
    if (!pipe || !buffer || count == 0) {
        return 0;
    }

    unsigned char *out = buffer;
    mutex_lock(&pipe->lock);
    size_t copied = 0;
    while (copied < count && pipe->size > 0) {
        out[copied++] = pipe->buffer[pipe->read_index];
        pipe->read_index = (pipe->read_index + 1) % pipe->capacity;
        --pipe->size;
    }
    mutex_unlock(&pipe->lock);
    return copied;
}

size_t pipe_write(ipc_pipe_t *pipe, const void *buffer, size_t count)
{
    if (!pipe || !buffer || count == 0 || pipe->reader_closed) {
        return 0;
    }

    const unsigned char *in = buffer;
    mutex_lock(&pipe->lock);
    size_t copied = 0;
    while (copied < count && pipe->size < pipe->capacity) {
        pipe->buffer[pipe->write_index] = in[copied++];
        pipe->write_index = (pipe->write_index + 1) % pipe->capacity;
        ++pipe->size;
    }
    mutex_unlock(&pipe->lock);
    return copied;
}

void pipe_close_reader(ipc_pipe_t *pipe)
{
    if (pipe) {
        pipe->reader_closed = true;
    }
}

void pipe_close_writer(ipc_pipe_t *pipe)
{
    if (pipe) {
        pipe->writer_closed = true;
    }
}

size_t pipe_available(const ipc_pipe_t *pipe)
{
    return pipe ? pipe->size : 0;
}

size_t pipe_space(const ipc_pipe_t *pipe)
{
    return pipe && pipe->capacity >= pipe->size ? pipe->capacity - pipe->size : 0;
}
