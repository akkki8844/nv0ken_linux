#include "semaphore.h"

void semaphore_init(semaphore_t *semaphore, int32_t initial, int32_t limit)
{
    if (!semaphore) {
        return;
    }

    if (limit <= 0) {
        limit = 1;
    }
    if (initial < 0) {
        initial = 0;
    }
    if (initial > limit) {
        initial = limit;
    }

    semaphore->count = initial;
    semaphore->limit = limit;
}

bool semaphore_try_wait(semaphore_t *semaphore)
{
    if (!semaphore) {
        return false;
    }

    for (;;) {
        int32_t count = semaphore->count;
        if (count <= 0) {
            return false;
        }
        if (__sync_bool_compare_and_swap(&semaphore->count, count, count - 1)) {
            return true;
        }
    }
}

void semaphore_wait(semaphore_t *semaphore)
{
    while (!semaphore_try_wait(semaphore)) {
        __asm__ volatile("pause");
    }
}

bool semaphore_post(semaphore_t *semaphore)
{
    if (!semaphore) {
        return false;
    }

    for (;;) {
        int32_t count = semaphore->count;
        if (count >= semaphore->limit) {
            return false;
        }
        if (__sync_bool_compare_and_swap(&semaphore->count, count, count + 1)) {
            return true;
        }
    }
}

int32_t semaphore_value(const semaphore_t *semaphore)
{
    return semaphore ? semaphore->count : 0;
}
