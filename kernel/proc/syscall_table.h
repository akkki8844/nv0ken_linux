#ifndef NV0KEN_PROC_SYSCALL_TABLE_H
#define NV0KEN_PROC_SYSCALL_TABLE_H

#include <stdint.h>

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_STAT = 4,
    SYS_FSTAT = 5,
    SYS_LSTAT = 6,
    SYS_SEEK = 7,
    SYS_BRK = 10,
    SYS_FORK = 11,
    SYS_EXECVE = 12,
    SYS_EXIT = 13,
    SYS_WAIT = 14,
    SYS_WAITPID = 15,
    SYS_GETPID = 16,
    SYS_GETPPID = 17,
    SYS_GETUID = 18,
    SYS_GETGID = 19,
    SYS_DUP = 20,
    SYS_DUP2 = 21,
    SYS_PIPE = 22,
    SYS_MKDIR = 25,
    SYS_RMDIR = 26,
    SYS_UNLINK = 27,
    SYS_CHDIR = 29,
    SYS_GETCWD = 30,
    SYS_OPENDIR = 31,
    SYS_READDIR = 32,
    SYS_CLOSEDIR = 33,
    SYS_TRUNCATE = 40,
    SYS_FTRUNCATE = 41,
    SYS_NANOSLEEP = 42,
    SYS_KILL = 45,
    SYS_SOCKET = 49,
    SYS_CONNECT = 51,
    SYS_SHMGET = 63,
    SYS_SHMAT = 64,
    SYS_SHMDT = 65,
    SYS_UNAME = 74,
    SYS_SYNC = 76,
    SYS_YIELD = 77,
    SYS_MAX = 128
};

void syscall_table_init(void);

#endif
