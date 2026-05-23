#include "syscall.h"

#include "gdt.h"
#include "msr.h"
#include "lib/log.h"

#define MAX_SYSCALLS 256
#define ENOSYS_VALUE 38

static syscall_fn_t syscall_table[MAX_SYSCALLS];

static uint64_t sys_ni(uint64_t a0, uint64_t a1, uint64_t a2,
                       uint64_t a3, uint64_t a4, uint64_t a5)
{
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    return (uint64_t)-ENOSYS_VALUE;
}

void syscall_init(void)
{
    for (unsigned i = 0; i < MAX_SYSCALLS; ++i) {
        syscall_table[i] = sys_ni;
    }

    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | 1u);
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
    wrmsr(MSR_STAR, ((uint64_t)GDT_USER_CODE << 48) |
                    ((uint64_t)GDT_KERNEL_CODE << 32));
    wrmsr(MSR_SFMASK, 0x200);
    log_info("syscall", "syscall/sysret entry configured");
}

void syscall_register(uint64_t number, syscall_fn_t handler)
{
    if (number >= MAX_SYSCALLS) {
        return;
    }
    syscall_table[number] = handler ? handler : sys_ni;
}

uint64_t syscall_dispatch(uint64_t number,
                          uint64_t arg0, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    if (number >= MAX_SYSCALLS) {
        return (uint64_t)-ENOSYS_VALUE;
    }
    return syscall_table[number](arg0, arg1, arg2, arg3, arg4, arg5);
}
