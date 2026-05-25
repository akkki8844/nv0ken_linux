#include "syscall_handlers.h"

#include "../lib/kprintf.h"
#include "process.h"
#include "scheduler.h"

long sys_write(uint64_t fd, uint64_t buffer, uint64_t length,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused4;
    (void)unused5;
    (void)unused6;

    if (fd != 1 && fd != 2) {
        return -9;
    }
    if (!buffer) {
        return -14;
    }

    const char *text = (const char *)(uintptr_t)buffer;
    for (uint64_t index = 0; index < length; ++index) {
        kputchar(text[index]);
    }
    return (long)length;
}

long sys_exit(uint64_t code, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    process_exit(process_current(), (int)code);
    scheduler_yield();
    return 0;
}

long sys_yield(uint64_t unused1, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    scheduler_yield();
    return 0;
}
