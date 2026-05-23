#ifndef NV0KEN_ARCH_X86_64_SYSCALL_H
#define NV0KEN_ARCH_X86_64_SYSCALL_H

#include <stdint.h>

typedef uint64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);

void syscall_init(void);
void syscall_register(uint64_t number, syscall_fn_t handler);
uint64_t syscall_dispatch(uint64_t number,
                          uint64_t arg0, uint64_t arg1, uint64_t arg2,
                          uint64_t arg3, uint64_t arg4, uint64_t arg5);
void syscall_entry(void);

#endif
