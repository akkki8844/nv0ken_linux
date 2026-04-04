#ifndef UNISTD_H
#define UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     close(int fd);
int     dup(int fd);
int     dup2(int oldfd, int newfd);
off_t   lseek(int fd, off_t offset, int whence);
int     pipe(int fds[2]);
int     access(const char *path, int mode);
int     unlink(const char *path);
int     rmdir(const char *path);
int     chdir(const char *path);
char   *getcwd(char *buf, size_t size);
int     symlink(const char *target, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int     link(const char *old, const char *new);
int     truncate(const char *path, off_t length);
int     ftruncate(int fd, off_t length);
unsigned int sleep(unsigned int seconds);
int     usleep(unsigned int usec);
pid_t   fork(void);
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
uid_t   geteuid(void);
gid_t   getgid(void);
gid_t   getegid(void);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execv(const char *path, char *const argv[]);
int     execvp(const char *file, char *const argv[]);
int     execl(const char *path, const char *arg, ...);
void   *sbrk(intptr_t increment);
int     isatty(int fd);
int     mount(const char *src, const char *target,
              const char *fstype, unsigned long flags);
int     umount(const char *target);
long    sysconf(int name);

#endif