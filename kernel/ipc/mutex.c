#include "mutex.h"

static inline uint64_t current_owner_token(void)
{
    return 1;
}

void mutex_init(mutex_t *mutex)
{
    if (!mutex) {
        return;
    }

    mutex->locked = 0;
    mutex->owner = 0;
    mutex->recursion = 0;
}

bool mutex_try_lock(mutex_t *mutex)
{
    if (!mutex) {
        return false;
    }

    uint64_t owner = current_owner_token();
    if (mutex->locked && mutex->owner == owner) {
        ++mutex->recursion;
        return true;
    }

    if (__sync_bool_compare_and_swap(&mutex->locked, 0, 1)) {
        mutex->owner = owner;
        mutex->recursion = 1;
        return true;
    }

    return false;
}

void mutex_lock(mutex_t *mutex)
{
    while (!mutex_try_lock(mutex)) {
        __asm__ volatile("pause");
    }
}

void mutex_unlock(mutex_t *mutex)
{
    if (!mutex || !mutex->locked || mutex->owner != current_owner_token()) {
        return;
    }

    if (--mutex->recursion == 0) {
        mutex->owner = 0;
        __sync_synchronize();
        mutex->locked = 0;
    }
}

bool mutex_is_locked(const mutex_t *mutex)
{
    return mutex && mutex->locked != 0;
}
