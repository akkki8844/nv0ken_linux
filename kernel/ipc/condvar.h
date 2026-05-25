#ifndef NV0KEN_IPC_CONDVAR_H
#define NV0KEN_IPC_CONDVAR_H

#include <stdint.h>

#include "mutex.h"

typedef struct {
    volatile uint64_t sequence;
} condvar_t;

void condvar_init(condvar_t *condvar);
void condvar_wait(condvar_t *condvar, mutex_t *mutex);
void condvar_signal(condvar_t *condvar);
void condvar_broadcast(condvar_t *condvar);

#endif
