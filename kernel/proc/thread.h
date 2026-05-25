#ifndef NV0KEN_PROC_THREAD_H
#define NV0KEN_PROC_THREAD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_SLEEPING,
    THREAD_EXITED
} thread_state_t;

typedef struct thread {
    uint64_t tid;
    uint64_t process_id;
    thread_state_t state;
    uintptr_t stack_top;
    uintptr_t instruction_pointer;
    int exit_code;
    uint64_t wake_tick;
    struct thread *next;
} thread_t;

void thread_system_init(void);
thread_t *thread_create(uint64_t process_id, uintptr_t entry, uintptr_t stack_top);
void thread_destroy(thread_t *thread);
void thread_set_state(thread_t *thread, thread_state_t state);
bool thread_is_runnable(const thread_t *thread);

#endif
