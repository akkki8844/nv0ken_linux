#include "scheduler.h"

static thread_t *run_queue;
static thread_t *current_thread;

void scheduler_init(void)
{
    run_queue = 0;
    current_thread = 0;
}

void scheduler_add(thread_t *thread)
{
    if (!thread) {
        return;
    }

    thread->state = THREAD_READY;
    thread->next = 0;
    if (!run_queue) {
        run_queue = thread;
        return;
    }

    thread_t *tail = run_queue;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = thread;
}

void scheduler_remove(thread_t *thread)
{
    thread_t **cursor = &run_queue;
    while (*cursor) {
        if (*cursor == thread) {
            *cursor = thread->next;
            thread->next = 0;
            if (current_thread == thread) {
                current_thread = 0;
            }
            return;
        }
        cursor = &(*cursor)->next;
    }
}

thread_t *scheduler_current(void)
{
    return current_thread;
}

thread_t *scheduler_pick_next(void)
{
    if (!run_queue) {
        current_thread = 0;
        return 0;
    }

    thread_t *start = current_thread ? current_thread->next : run_queue;
    if (!start) {
        start = run_queue;
    }

    thread_t *candidate = start;
    do {
        if (thread_is_runnable(candidate)) {
            current_thread = candidate;
            current_thread->state = THREAD_RUNNING;
            return current_thread;
        }
        candidate = candidate->next ? candidate->next : run_queue;
    } while (candidate != start);

    return 0;
}

void scheduler_yield(void)
{
    if (current_thread && current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;
    }
    scheduler_pick_next();
}

size_t scheduler_runnable_count(void)
{
    size_t count = 0;
    for (thread_t *thread = run_queue; thread; thread = thread->next) {
        if (thread_is_runnable(thread)) {
            ++count;
        }
    }
    return count;
}
