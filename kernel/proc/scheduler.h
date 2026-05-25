#ifndef NV0KEN_PROC_SCHEDULER_H
#define NV0KEN_PROC_SCHEDULER_H

#include <stddef.h>

#include "thread.h"

void scheduler_init(void);
void scheduler_add(thread_t *thread);
void scheduler_remove(thread_t *thread);
thread_t *scheduler_current(void);
thread_t *scheduler_pick_next(void);
void scheduler_yield(void);
size_t scheduler_runnable_count(void);

#endif
