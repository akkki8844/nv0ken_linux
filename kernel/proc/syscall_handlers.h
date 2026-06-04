#ifndef NV0KEN_PROC_SYSCALL_HANDLERS_H
#define NV0KEN_PROC_SYSCALL_HANDLERS_H

#include <stdint.h>

long sys_read(uint64_t fd, uint64_t buffer, uint64_t length,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_write(uint64_t fd, uint64_t buffer, uint64_t length,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_open(uint64_t path, uint64_t flags, uint64_t mode,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_close(uint64_t fd, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_stat(uint64_t path, uint64_t statbuf, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_fstat(uint64_t fd, uint64_t statbuf, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_seek(uint64_t fd, uint64_t offset, uint64_t whence,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_brk(uint64_t address, uint64_t unused2, uint64_t unused3,
             uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_exit(uint64_t code, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_wait(uint64_t result, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_waitpid(uint64_t pid, uint64_t status, uint64_t options,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_getpid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_getppid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_getuid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_getgid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_dup(uint64_t fd, uint64_t unused2, uint64_t unused3,
             uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_pipe(uint64_t fds, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_mkdir(uint64_t path, uint64_t mode, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_unlink(uint64_t path, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_chdir(uint64_t path, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_getcwd(uint64_t buffer, uint64_t size, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_opendir(uint64_t path, uint64_t unused2, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_readdir(uint64_t fd, uint64_t entry, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_truncate(uint64_t path, uint64_t length, uint64_t unused3,
                  uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_ftruncate(uint64_t fd, uint64_t length, uint64_t unused3,
                   uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_nanosleep(uint64_t req, uint64_t rem, uint64_t unused3,
                   uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_kill(uint64_t pid, uint64_t signal, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_socket(uint64_t domain, uint64_t type, uint64_t protocol,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_connect(uint64_t fd, uint64_t addr, uint64_t addrlen,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_shmget(uint64_t key, uint64_t size, uint64_t flags,
                uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_shmat(uint64_t shmid, uint64_t addr, uint64_t flags,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_shmdt(uint64_t addr, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_uname(uint64_t uts, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_sync(uint64_t unused1, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6);
long sys_yield(uint64_t unused1, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6);

#endif
