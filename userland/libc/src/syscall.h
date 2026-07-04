#ifndef NV0_LIBC_SYSCALL_H
#define NV0_LIBC_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <types.h>

long sys_read(int fd, void *buffer, size_t count);
long sys_write(int fd, const void *buffer, size_t count);
long sys_close(int fd);
long sys_seek(int fd, off_t offset, int whence);
long sys_brk(intptr_t address);
long sys_fork(void);
long sys_execve(const char *path, char *const argv[], char *const envp[]);
_Noreturn void sys_exit(int status);
long sys_getpid(void);
long sys_getppid(void);
long sys_getuid(void);
long sys_getgid(void);
long sys_dup(int fd);
long sys_dup2(int old_fd, int new_fd);
long sys_pipe(int pipe_fds[2]);
long sys_stat(const char *path, void *stat_buffer);
long sys_rmdir(const char *path);
long sys_unlink(const char *path);
long sys_rename(const char *old_path, const char *new_path);
long sys_chdir(const char *path);
long sys_getcwd(char *buffer, size_t size);
long sys_symlink(const char *target, const char *link_path);
long sys_readlink(const char *path, char *buffer, size_t size);
long sys_mount(const char *source, const char *target, const char *filesystem);
long sys_umount(const char *target);
long sys_truncate(const char *path, off_t length);
long sys_ftruncate(int fd, off_t length);

#endif
