#ifndef NV0KEN_PROC_PROCESS_H
#define NV0KEN_PROC_PROCESS_H

#include <stdbool.h>
#include <stdint.h>

#include "../ipc/signal.h"
#include "../mm/addr_space.h"
#include "thread.h"

#define PROCESS_NAME_MAX 32

typedef enum {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_WAITING,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct process {
    uint64_t pid;
    uint64_t parent_pid;
    char name[PROCESS_NAME_MAX];
    process_state_t state;
    int exit_code;
    addr_space_t *address_space;
    signal_state_t signals;
    thread_t *main_thread;
    struct process *next;
} process_t;

void process_system_init(void);
process_t *process_create(const char *name, uint64_t parent_pid);
void process_destroy(process_t *process);
process_t *process_find(uint64_t pid);
process_t *process_current(void);
void process_set_current(process_t *process);
void process_exit(process_t *process, int code);
bool process_is_alive(const process_t *process);

#endif
