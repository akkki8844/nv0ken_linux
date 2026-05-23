#ifndef NV0KEN_LIB_SPINLOCK_H
#define NV0KEN_LIB_SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

void spinlock_init(spinlock_t *lock);
void spin_lock(spinlock_t *lock);
int  spin_trylock(spinlock_t *lock);
void spin_unlock(spinlock_t *lock);
int  spin_is_locked(const spinlock_t *lock);

#endif
