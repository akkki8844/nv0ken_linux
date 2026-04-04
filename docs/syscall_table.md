# syscall table

syscalls are invoked via the x86_64 `SYSCALL` instruction. the calling convention matches Linux x86_64:

```
rax   syscall number
rdi   arg1
rsi   arg2
rdx   arg3
r10   arg4
r8    arg5
r9    arg6

return value in rax
negative rax = errno (negated)
```

## table

| number | name            | signature                                              | description                        |
|--------|-----------------|--------------------------------------------------------|------------------------------------|
| 0      | sys_read        | (int fd, void *buf, size_t len)                        | read from fd                       |
| 1      | sys_write       | (int fd, const void *buf, size_t len)                  | write to fd                        |
| 2      | sys_open        | (const char *path, int flags, int mode)                | open file, return fd               |
| 3      | sys_close       | (int fd)                                               | close fd                           |
| 4      | sys_stat        | (const char *path, struct stat *buf)                   | get file metadata                  |
| 5      | sys_fstat       | (int fd, struct stat *buf)                             | stat on open fd                    |
| 6      | sys_lstat       | (const char *path, struct stat *buf)                   | stat without following symlinks    |
| 7      | sys_seek        | (int fd, off_t offset, int whence)                     | reposition file offset             |
| 8      | sys_mmap        | (void *addr, size_t len, int prot, int flags, int fd, off_t off) | map memory          |
| 9      | sys_munmap      | (void *addr, size_t len)                               | unmap memory                       |
| 10     | sys_brk         | (void *addr)                                           | set heap break                     |
| 11     | sys_fork        | (void)                                                 | fork process, return PID           |
| 12     | sys_execve      | (const char *path, char **argv, char **envp)           | replace process image              |
| 13     | sys_exit        | (int status)                                           | terminate process                  |
| 14     | sys_wait        | (int *status)                                          | wait for any child                 |
| 15     | sys_waitpid     | (pid_t pid, int *status, int options)                  | wait for specific child            |
| 16     | sys_getpid      | (void)                                                 | return current PID                 |
| 17     | sys_getppid     | (void)                                                 | return parent PID                  |
| 18     | sys_getuid      | (void)                                                 | return user ID                     |
| 19     | sys_getgid      | (void)                                                 | return group ID                    |
| 20     | sys_dup         | (int fd)                                               | duplicate fd                       |
| 21     | sys_dup2        | (int old, int new)                                     | duplicate fd to specific number    |
| 22     | sys_pipe        | (int fds[2])                                           | create anonymous pipe              |
| 23     | sys_fcntl       | (int fd, int cmd, int arg)                             | file control                       |
| 24     | sys_ioctl       | (int fd, unsigned long req, void *arg)                 | device control                     |
| 25     | sys_mkdir       | (const char *path, int mode)                           | create directory                   |
| 26     | sys_rmdir       | (const char *path)                                     | remove empty directory             |
| 27     | sys_unlink      | (const char *path)                                     | remove file                        |
| 28     | sys_rename      | (const char *old, const char *new)                     | rename file or directory           |
| 29     | sys_chdir       | (const char *path)                                     | change working directory           |
| 30     | sys_getcwd      | (char *buf, size_t size)                               | get working directory path         |
| 31     | sys_opendir     | (const char *path)                                     | open directory for reading         |
| 32     | sys_readdir     | (int fd, struct dirent *buf)                           | read next directory entry          |
| 33     | sys_closedir    | (int fd)                                               | close directory fd                 |
| 34     | sys_symlink     | (const char *target, const char *link)                 | create symbolic link               |
| 35     | sys_readlink    | (const char *path, char *buf, size_t len)              | read symlink target                |
| 36     | sys_chmod       | (const char *path, int mode)                           | change file permissions            |
| 37     | sys_chown       | (const char *path, int uid, int gid)                   | change file ownership              |
| 38     | sys_mount       | (const char *dev, const char *target, const char *fs)  | mount filesystem                   |
| 39     | sys_umount      | (const char *target)                                   | unmount filesystem                 |
| 40     | sys_truncate    | (const char *path, off_t len)                          | truncate file                      |
| 41     | sys_ftruncate   | (int fd, off_t len)                                    | truncate open file                 |
| 42     | sys_nanosleep   | (const struct timespec *req, struct timespec *rem)     | sleep with nanosecond precision    |
| 43     | sys_time        | (time_t *t)                                            | get Unix timestamp                 |
| 44     | sys_clock_gettime | (int clkid, struct timespec *tp)                     | get clock time                     |
| 45     | sys_kill        | (pid_t pid, int sig)                                   | send signal to process             |
| 46     | sys_sigaction   | (int sig, const struct sigaction *act, struct sigaction *old) | set signal handler        |
| 47     | sys_sigprocmask | (int how, const sigset_t *set, sigset_t *old)          | block/unblock signals              |
| 48     | sys_sigreturn   | (void)                                                 | return from signal handler         |
| 49     | sys_socket      | (int domain, int type, int protocol)                   | create socket                      |
| 50     | sys_bind        | (int fd, const struct sockaddr *addr, int addrlen)     | bind socket to address             |
| 51     | sys_connect     | (int fd, const struct sockaddr *addr, int addrlen)     | connect socket                     |
| 52     | sys_listen      | (int fd, int backlog)                                  | mark socket as passive             |
| 53     | sys_accept      | (int fd, struct sockaddr *addr, int *addrlen)          | accept connection                  |
| 54     | sys_send        | (int fd, const void *buf, size_t len, int flags)       | send data on socket                |
| 55     | sys_recv        | (int fd, void *buf, size_t len, int flags)             | receive data on socket             |
| 56     | sys_sendto      | (int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, int addrlen) | UDP send |
| 57     | sys_recvfrom    | (int fd, void *buf, size_t len, int flags, struct sockaddr *addr, int *addrlen) | UDP recv |
| 58     | sys_shutdown    | (int fd, int how)                                      | shutdown socket                    |
| 59     | sys_setsockopt  | (int fd, int level, int opt, const void *val, int len) | set socket option                  |
| 60     | sys_getsockopt  | (int fd, int level, int opt, void *val, int *len)      | get socket option                  |
| 61     | sys_pty_open    | (int fds[2])                                           | open PTY master/slave pair         |
| 62     | sys_pty_setsize | (int fd, int cols, int rows)                           | set PTY dimensions                 |
| 63     | sys_shmget      | (int key, size_t size, int flags)                      | create/get shared memory segment   |
| 64     | sys_shmat       | (int shmid, void *addr, int flags)                     | attach shared memory               |
| 65     | sys_shmdt       | (void *addr)                                           | detach shared memory               |
| 66     | sys_shmctl      | (int shmid, int cmd, void *buf)                        | shared memory control              |
| 67     | sys_semget      | (int key, int nsems, int flags)                        | create/get semaphore set           |
| 68     | sys_semop       | (int semid, struct sembuf *ops, int nops)              | semaphore operation                |
| 69     | sys_semctl      | (int semid, int semnum, int cmd, int val)              | semaphore control                  |
| 70     | sys_msgget      | (int key, int flags)                                   | create/get message queue           |
| 71     | sys_msgsnd      | (int msqid, const void *msg, size_t size, int flags)   | send message                       |
| 72     | sys_msgrcv      | (int msqid, void *msg, size_t size, int type, int flags) | receive message                  |
| 73     | sys_msgctl      | (int msqid, int cmd, void *buf)                        | message queue control              |
| 74     | sys_uname       | (struct utsname *buf)                                  | get system info                    |
| 75     | sys_reboot      | (int cmd)                                              | reboot or halt system              |
| 76     | sys_sync        | (void)                                                 | flush filesystem buffers           |

## errno values

| value | name          | meaning                    |
|-------|---------------|----------------------------|
| 1     | EPERM         | operation not permitted    |
| 2     | ENOENT        | no such file or directory  |
| 3     | ESRCH         | no such process            |
| 4     | EINTR         | interrupted syscall        |
| 5     | EIO           | I/O error                  |
| 6     | ENXIO         | no such device             |
| 7     | E2BIG         | argument list too long     |
| 8     | ENOEXEC       | exec format error          |
| 9     | EBADF         | bad file descriptor        |
| 10    | ECHILD        | no child processes         |
| 11    | EAGAIN        | try again                  |
| 12    | ENOMEM        | out of memory              |
| 13    | EACCES        | permission denied          |
| 14    | EFAULT        | bad address                |
| 16    | EBUSY         | device or resource busy    |
| 17    | EEXIST        | file exists                |
| 18    | EXDEV         | cross-device link          |
| 19    | ENODEV        | no such device             |
| 20    | ENOTDIR       | not a directory            |
| 21    | EISDIR        | is a directory             |
| 22    | EINVAL        | invalid argument           |
| 23    | ENFILE        | file table overflow        |
| 24    | EMFILE        | too many open files        |
| 25    | ENOTTY        | not a typewriter           |
| 27    | EFBIG         | file too large             |
| 28    | ENOSPC        | no space left on device    |
| 29    | ESPIPE        | illegal seek               |
| 30    | EROFS         | read-only filesystem       |
| 32    | EPIPE         | broken pipe                |
| 33    | EDOM          | math argument out of range |
| 34    | ERANGE        | math result not representable |
| 36    | EDEADLK       | resource deadlock          |
| 38    | ENOSYS        | function not implemented   |
| 39    | ENOTEMPTY     | directory not empty        |
| 40    | ELOOP         | too many symbolic links    |
| 110   | ETIMEDOUT     | connection timed out       |
| 111   | ECONNREFUSED  | connection refused         |
| 113   | EHOSTUNREACH  | no route to host           |