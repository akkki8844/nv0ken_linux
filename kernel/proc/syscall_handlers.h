#ifndef NV0KEN_PROC_SYSCALL_HANDLERS_H
#define NV0KEN_PROC_SYSCALL_HANDLERS_H

#include <stdint.h>

long sys_write(uint64_t fd, uint64_t buffer, uint64_t length,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_exit(uint64_t code, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_yield(uint64_t unused1, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);

#endif
