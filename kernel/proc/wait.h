#ifndef NV0KEN_PROC_WAIT_H
#define NV0KEN_PROC_WAIT_H

#include <stdint.h>

#include "process.h"

typedef struct {
    uint64_t pid;
    int exit_code;
} wait_result_t;

int wait_for_pid(uint64_t pid, wait_result_t *result);
int wait_any_child(uint64_t parent_pid, wait_result_t *result);

#endif
