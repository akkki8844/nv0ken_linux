#ifndef NV0KEN_PROC_SYSCALL_TABLE_H
#define NV0KEN_PROC_SYSCALL_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef long (*syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_EXIT = 60,
    SYS_YIELD = 100,
    SYS_MAX = 128
};

void syscall_table_init(void);
void syscall_register(size_t number, syscall_fn_t handler);
long syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6);

#endif
