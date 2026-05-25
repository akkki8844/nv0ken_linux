#ifndef NV0KEN_IPC_MSGQUEUE_H
#define NV0KEN_IPC_MSGQUEUE_H

#include <stddef.h>
#include <stdint.h>

#include "mutex.h"

#define MSGQUEUE_MAX_PAYLOAD 256

typedef struct {
    uint32_t type;
    size_t length;
    unsigned char payload[MSGQUEUE_MAX_PAYLOAD];
} ipc_message_t;

typedef struct {
    ipc_message_t *messages;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    mutex_t lock;
} msgqueue_t;

int msgqueue_init(msgqueue_t *queue, size_t capacity);
void msgqueue_destroy(msgqueue_t *queue);
int msgqueue_send(msgqueue_t *queue, uint32_t type, const void *payload, size_t length);
int msgqueue_receive(msgqueue_t *queue, ipc_message_t *message);
size_t msgqueue_count(const msgqueue_t *queue);

#endif
