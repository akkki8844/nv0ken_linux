#include "msgqueue.h"

#include "../lib/string.h"
#include "../mm/heap.h"

int msgqueue_init(msgqueue_t *queue, size_t capacity)
{
    if (!queue || capacity == 0) {
        return -1;
    }

    queue->messages = kcalloc(capacity, sizeof(ipc_message_t));
    if (!queue->messages) {
        return -1;
    }

    queue->capacity = capacity;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    mutex_init(&queue->lock);
    return 0;
}

void msgqueue_destroy(msgqueue_t *queue)
{
    if (!queue) {
        return;
    }

    kfree(queue->messages);
    queue->messages = 0;
    queue->capacity = 0;
    queue->count = 0;
}

int msgqueue_send(msgqueue_t *queue, uint32_t type, const void *payload, size_t length)
{
    if (!queue || !queue->messages || length > MSGQUEUE_MAX_PAYLOAD) {
        return -1;
    }

    mutex_lock(&queue->lock);
    if (queue->count == queue->capacity) {
        mutex_unlock(&queue->lock);
        return -1;
    }

    ipc_message_t *message = &queue->messages[queue->tail];
    message->type = type;
    message->length = length;
    if (payload && length) {
        memcpy(message->payload, payload, length);
    }
    queue->tail = (queue->tail + 1) % queue->capacity;
    ++queue->count;
    mutex_unlock(&queue->lock);
    return 0;
}

int msgqueue_receive(msgqueue_t *queue, ipc_message_t *message)
{
    if (!queue || !queue->messages || !message) {
        return -1;
    }

    mutex_lock(&queue->lock);
    if (queue->count == 0) {
        mutex_unlock(&queue->lock);
        return -1;
    }

    *message = queue->messages[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    --queue->count;
    mutex_unlock(&queue->lock);
    return 0;
}

size_t msgqueue_count(const msgqueue_t *queue)
{
    return queue ? queue->count : 0;
}
