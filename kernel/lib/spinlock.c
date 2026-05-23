#include "spinlock.h"

void spinlock_init(spinlock_t *lock)
{
    if (lock) {
        lock->locked = 0;
    }
}

void spin_lock(spinlock_t *lock)
{
    while (!spin_trylock(lock)) {
        __asm__ volatile("pause");
    }
}

int spin_trylock(spinlock_t *lock)
{
    if (!lock) {
        return 0;
    }
    return __sync_bool_compare_and_swap(&lock->locked, 0, 1);
}

void spin_unlock(spinlock_t *lock)
{
    if (!lock) {
        return;
    }
    __sync_synchronize();
    lock->locked = 0;
}

int spin_is_locked(const spinlock_t *lock)
{
    return lock && lock->locked != 0;
}
