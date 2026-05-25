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
    for (process_t *process = process_first(); process; process = process->next) {
        if (process->parent_pid == parent_pid && process->state == PROCESS_ZOMBIE) {
            return wait_for_pid(process->pid, result);
        }
    }
    return -1;
}
