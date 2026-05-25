#ifndef NV0KEN_IPC_MUTEX_H
#define NV0KEN_IPC_MUTEX_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
    uint64_t owner;
    uint32_t recursion;
} mutex_t;

void mutex_init(mutex_t *mutex);
bool mutex_try_lock(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
bool mutex_is_locked(const mutex_t *mutex);

#endif
