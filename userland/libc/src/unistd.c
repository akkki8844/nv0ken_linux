#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

char **environ = 0;

static void errno_check(long ret) {
    if (ret < 0 && ret > -4096) {
        errno = (int)-ret;
    }
}

ssize_t read(int fd, void *buf, size_t count) {
    long r = sys_read(fd, buf, count);
    errno_check(r);
    return r < 0 ? -1 : (ssize_t)r;
}

ssize_t write(int fd, const void *buf, size_t count) {
    long r = sys_write(fd, buf, count);
    errno_check(r);
    return r < 0 ? -1 : (ssize_t)r;
}

int close(int fd) {
    long r = sys_close(fd);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

int dup(int fd) {
    long r = sys_dup(fd);
    errno_check(r);
    return (int)r;
}

int dup2(int oldfd, int newfd) {
    long r = sys_dup2(oldfd, newfd);
    errno_check(r);
    return (int)r;
}

off_t lseek(int fd, off_t offset, int whence) {
    long r = sys_seek(fd, offset, whence);
    errno_check(r);
    return r < 0 ? (off_t)-1 : (off_t)r;
}

int pipe(int fds[2]) {
    long r = sys_pipe(fds);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

int access(const char *path, int mode) {
    struct stat st;
    if (sys_stat(path, &st) < 0) return -1;
    (void)mode;
    return 0;
}

int unlink(const char *path) {
    long r = sys_unlink(path);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

int rmdir(const char *path) {
    long r = sys_rmdir(path);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

int chdir(const char *path) {
    long r = sys_chdir(path);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

char *getcwd(char *buf, size_t size) {
    long r = sys_getcwd(buf, size);
    errno_check(r);
    return r < 0 ? 0 : buf;
}

int symlink(const char *target, const char *linkpath) {
    long r = sys_symlink(target, linkpath);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    long r = sys_readlink(path, buf, bufsiz);
    errno_check(r);
    return r < 0 ? -1 : (ssize_t)r;
}

int link(const char *old, const char *new) {
    (void)old; (void)new;
    errno = ENOSYS;
    return -1;
}

int truncate(const char *path, off_t length) {
    long r = sys_truncate(path, length);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

int ftruncate(int fd, off_t length) {
    long r = sys_ftruncate(fd, length);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

unsigned int sleep(unsigned int seconds) {
    struct timespec req = { (long long)seconds, 0 };
    sys_nanosleep(&req, 0);
    return 0;
}

int usleep(unsigned int usec) {
    struct timespec req = { 0, (long long)usec * 1000 };
    sys_nanosleep(&req, 0);
    return 0;
}

pid_t fork(void) {
    long r = sys_fork();
    errno_check(r);
    return (pid_t)r;
}

pid_t getpid(void) {
    return (pid_t)sys_getpid();
}

pid_t getppid(void) {
    return (pid_t)sys_getppid();
}

uid_t getuid(void) {
    return (uid_t)sys_getuid();
}

uid_t geteuid(void) {
    return (uid_t)sys_getuid();
}

gid_t getgid(void) {
    return (gid_t)sys_getgid();
}

gid_t getegid(void) {
    return (gid_t)sys_getgid();
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    long r = sys_execve(path, argv, envp);
    errno_check(r);
    return -1;
}

int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

int execvp(const char *file, char *const argv[]) {
    return execve(file, argv, environ);
}

int execl(const char *path, const char *arg, ...) {
    va_list ap;
    char *args[64];
    int n = 0;
    args[n++] = (char *)arg;
    va_start(ap, arg);
    while ((args[n] = va_arg(ap, char *)) != 0) n++;
    va_end(ap);
    args[n] = 0;
    return execve(path, args, environ);
}

void *sbrk(intptr_t increment) {
    long r = sys_brk(0);
    if (r < 0) return (void *)-1;
    long new_brk = sys_brk(r + increment);
    if (new_brk < 0) return (void *)-1;
    return (void *)(long)(r);
}

int isatty(int fd) {
    (void)fd;
    return 1;
}

int mount(const char *src, const char *target,
          const char *fstype, unsigned long flags) {
    long r = sys_mount(src, target, fstype);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

int umount(const char *target) {
    long r = sys_umount(target);
    errno_check(r);
    return r < 0 ? -1 : 0;
}

long sysconf(int name) {
    (void)name;
    return -1;
}