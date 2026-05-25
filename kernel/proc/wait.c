#include "wait.h"

int wait_for_pid(uint64_t pid, wait_result_t *result)
{
    process_t *process = process_find(pid);
    if (!process || process->state != PROCESS_ZOMBIE) {
        return -1;
    }

    if (result) {
        result->pid = process->pid;
        result->exit_code = process->exit_code;
    }
    process_destroy(process);
    return 0;
}

int wait_any_child(uint64_t parent_pid, wait_result_t *result)
{
    for (uint64_t pid = 1; pid < UINT64_MAX; ++pid) {
        process_t *process = process_find(pid);
        if (!process) {
            continue;
        }
        if (process->parent_pid == parent_pid && process->state == PROCESS_ZOMBIE) {
            return wait_for_pid(pid, result);
        }
    }
    return -1;
}
