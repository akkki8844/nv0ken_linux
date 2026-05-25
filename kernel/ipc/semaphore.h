#ifndef NV0KEN_IPC_SEMAPHORE_H
#define NV0KEN_IPC_SEMAPHORE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile int32_t count;
    int32_t limit;
} semaphore_t;

void semaphore_init(semaphore_t *semaphore, int32_t initial, int32_t limit);
bool semaphore_try_wait(semaphore_t *semaphore);
void semaphore_wait(semaphore_t *semaphore);
bool semaphore_post(semaphore_t *semaphore);
int32_t semaphore_value(const semaphore_t *semaphore);

#endif
