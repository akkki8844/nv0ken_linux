#ifndef NV0KEN_IPC_PIPE_H
#define NV0KEN_IPC_PIPE_H

#include <stdbool.h>
#include <stddef.h>

#include "mutex.h"

#define IPC_PIPE_DEFAULT_CAPACITY 4096

typedef struct {
    unsigned char *buffer;
    size_t capacity;
    size_t read_index;
    size_t write_index;
    size_t size;
    bool reader_closed;
    bool writer_closed;
    mutex_t lock;
} ipc_pipe_t;

int pipe_init(ipc_pipe_t *pipe, size_t capacity);
void pipe_destroy(ipc_pipe_t *pipe);
size_t pipe_read(ipc_pipe_t *pipe, void *buffer, size_t count);
size_t pipe_write(ipc_pipe_t *pipe, const void *buffer, size_t count);
void pipe_close_reader(ipc_pipe_t *pipe);
void pipe_close_writer(ipc_pipe_t *pipe);
size_t pipe_available(const ipc_pipe_t *pipe);
size_t pipe_space(const ipc_pipe_t *pipe);

#endif
