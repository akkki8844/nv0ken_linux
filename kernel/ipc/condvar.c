#include "condvar.h"

void condvar_init(condvar_t *condvar)
{
    if (condvar) {
        condvar->sequence = 0;
    }
}

void condvar_wait(condvar_t *condvar, mutex_t *mutex)
{
    if (!condvar || !mutex) {
        return;
    }

    uint64_t observed = condvar->sequence;
    mutex_unlock(mutex);
    while (condvar->sequence == observed) {
        __asm__ volatile("pause");
    }
    mutex_lock(mutex);
}

void condvar_signal(condvar_t *condvar)
{
    if (condvar) {
        __sync_add_and_fetch(&condvar->sequence, 1);
    }
}

void condvar_broadcast(condvar_t *condvar)
{
    condvar_signal(condvar);
}
