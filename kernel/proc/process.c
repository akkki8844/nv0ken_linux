#include "process.h"

#include "../lib/string.h"
#include "../mm/heap.h"

static process_t *process_list;
static process_t *current_process;
static uint64_t next_pid = 1;

void process_system_init(void)
{
    process_list = 0;
    current_process = 0;
    next_pid = 1;
    thread_system_init();
}

process_t *process_create(const char *name, uint64_t parent_pid)
{
    process_t *process = kcalloc(1, sizeof(process_t));
    if (!process) {
        return 0;
    }

    process->pid = next_pid++;
    process->parent_pid = parent_pid;
    process->state = PROCESS_NEW;
    process->address_space = addr_space_kernel();
    signal_state_init(&process->signals);

    if (name) {
        size_t length = strlen(name);
        if (length >= PROCESS_NAME_MAX) {
            length = PROCESS_NAME_MAX - 1;
        }
        memcpy(process->name, name, length);
        process->name[length] = '\0';
    }

    process->next = process_list;
    process_list = process;
    if (!current_process) {
        current_process = process;
    }
    return process;
}

void process_destroy(process_t *process)
{
    if (!process) {
        return;
    }

    process_t **cursor = &process_list;
    while (*cursor) {
        if (*cursor == process) {
            *cursor = process->next;
            break;
        }
        cursor = &(*cursor)->next;
    }

    if (current_process == process) {
        current_process = process_list;
    }
    thread_destroy(process->main_thread);
    kfree(process);
}

process_t *process_find(uint64_t pid)
{
    for (process_t *process = process_list; process; process = process->next) {
        if (process->pid == pid) {
            return process;
        }
    }
    return 0;
}

process_t *process_current(void)
{
    return current_process;
}

process_t *process_first(void)
{
    return process_list;
}

void process_set_current(process_t *process)
{
    current_process = process;
}

void process_exit(process_t *process, int code)
{
    if (!process) {
        process = current_process;
    }
    if (!process) {
        return;
    }

    process->exit_code = code;
    process->state = PROCESS_ZOMBIE;
    if (process->main_thread) {
        process->main_thread->exit_code = code;
        process->main_thread->state = THREAD_EXITED;
    }
}

bool process_is_alive(const process_t *process)
{
    return process && process->state != PROCESS_ZOMBIE;
}
