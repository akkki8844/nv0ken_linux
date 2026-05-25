#include "syscall_table.h"

#include "syscall_handlers.h"

static syscall_fn_t syscall_table[SYS_MAX];

static long sys_unimplemented(uint64_t arg1, uint64_t arg2, uint64_t arg3,
                              uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    return -38;
}

void syscall_table_init(void)
{
    for (size_t index = 0; index < SYS_MAX; ++index) {
        syscall_table[index] = sys_unimplemented;
    }

    syscall_register(SYS_WRITE, sys_write);
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_YIELD, sys_yield);
}

void syscall_register(size_t number, syscall_fn_t handler)
{
    if (number < SYS_MAX) {
        syscall_table[number] = handler ? handler : sys_unimplemented;
    }
}

long syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    if (number >= SYS_MAX) {
        return -38;
    }
    return syscall_table[number](arg1, arg2, arg3, arg4, arg5, arg6);
}
