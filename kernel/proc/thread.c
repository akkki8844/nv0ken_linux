#include "thread.h"

#include "../lib/string.h"
#include "../mm/heap.h"

static uint64_t next_tid = 1;

void thread_system_init(void)
{
    next_tid = 1;
}

thread_t *thread_create(uint64_t process_id, uintptr_t entry, uintptr_t stack_top)
{
    thread_t *thread = kcalloc(1, sizeof(thread_t));
    if (!thread) {
        return 0;
    }

    thread->tid = next_tid++;
    thread->process_id = process_id;
    thread->state = THREAD_NEW;
    thread->instruction_pointer = entry;
    thread->stack_top = stack_top;
    return thread;
}

void thread_destroy(thread_t *thread)
{
    kfree(thread);
}

void thread_set_state(thread_t *thread, thread_state_t state)
{
    if (thread) {
        thread->state = state;
    }
}

bool thread_is_runnable(const thread_t *thread)
{
    return thread && (thread->state == THREAD_READY || thread->state == THREAD_RUNNING);
}
