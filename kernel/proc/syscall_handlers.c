#include "syscall_handlers.h"

#include <stddef.h>

#include "../drivers/timer.h"
#include "../fs/fd_table.h"
#include "../fs/pipe_fs.h"
#include "../fs/vfs.h"
#include "../ipc/shm.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"
#include "../mm/heap.h"
#include "../net/socket.h"
#include "process.h"
#include "scheduler.h"
#include "wait.h"

#define EINVAL 22
#define EBADF 9
#define EFAULT 14
#define ENOENT 2
#define ENOMEM 12
#define ENOSYS 38
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#define O_APPEND 0x0400
#define O_DIRECTORY 0x10000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_IFIFO 0010000
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    int64_t st_size;
    int64_t st_blksize;
    int64_t st_blocks;
    int64_t st_atime;
    int64_t st_mtime;
    int64_t st_ctime;
} user_stat_t;

typedef struct {
    uint64_t d_ino;
    unsigned char d_type;
    char d_name[256];
} user_dirent_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} user_timespec_t;

typedef struct {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
} user_utsname_t;

static char cwd[256] = "/";
static uintptr_t process_break = 0x40000000;

static long errno_ret(int err)
{
    return -err;
}

static uint32_t node_mode(const vfs_node_t *node)
{
    if (!node) {
        return 0;
    }
    if (node->mode) {
        return node->mode;
    }
    if (node->type == VFS_NODE_DIR) {
        return S_IFDIR | 0755;
    }
    if (node->type == VFS_NODE_PIPE) {
        return S_IFIFO | 0600;
    }
    return S_IFREG | 0644;
}

static unsigned char dirent_type(const vfs_node_t *node)
{
    if (!node) {
        return 0;
    }
    if (node->type == VFS_NODE_DIR) {
        return 4;
    }
    if (node->type == VFS_NODE_PIPE) {
        return 1;
    }
    return 8;
}

static long fill_stat(vfs_node_t *node, user_stat_t *statbuf)
{
    if (!node || !statbuf) {
        return errno_ret(EFAULT);
    }

    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_ino = (uint64_t)(uintptr_t)node;
    statbuf->st_mode = node_mode(node);
    statbuf->st_nlink = node->type == VFS_NODE_DIR ? 2 : 1;
    statbuf->st_size = (int64_t)node->size;
    statbuf->st_blksize = 4096;
    statbuf->st_blocks = (int64_t)((node->size + 511) / 512);
    return 0;
}

static int make_parent_dirs(const char *path)
{
    char copy[VFS_PATH_MAX];
    if (!path || strlen(path) >= sizeof(copy)) {
        return -1;
    }
    strcpy(copy, path);

    for (char *cursor = copy + 1; *cursor; ++cursor) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (!vfs_lookup(copy)) {
            vfs_create(copy, VFS_NODE_DIR, 0);
        }
        *cursor = '/';
    }
    return 0;
}

long sys_read(uint64_t fd, uint64_t buffer, uint64_t length,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused4;
    (void)unused5;
    (void)unused6;

    if (!buffer && length) {
        return errno_ret(EFAULT);
    }
    if (fd == 0) {
        return 0;
    }
    int read = fd_read((int)fd, (void *)(uintptr_t)buffer, (size_t)length);
    return read < 0 ? errno_ret(EBADF) : read;
}

long sys_write(uint64_t fd, uint64_t buffer, uint64_t length,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused4;
    (void)unused5;
    (void)unused6;

    if (!buffer) {
        return errno_ret(EFAULT);
    }

    if (fd == 1 || fd == 2) {
        const char *text = (const char *)(uintptr_t)buffer;
        for (uint64_t index = 0; index < length; ++index) {
            kputchar(text[index]);
        }
        return (long)length;
    }

    int written = fd_write((int)fd, (const void *)(uintptr_t)buffer, (size_t)length);
    return written < 0 ? errno_ret(EBADF) : written;
}

long sys_open(uint64_t path_arg, uint64_t flags, uint64_t mode,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)mode;
    (void)unused4;
    (void)unused5;
    (void)unused6;

    const char *path = (const char *)(uintptr_t)path_arg;
    if (!path) {
        return errno_ret(EFAULT);
    }

    vfs_node_t *node = vfs_lookup(path);
    if (!node && (flags & O_CREAT)) {
        make_parent_dirs(path);
        if (vfs_create(path, (flags & O_DIRECTORY) ? VFS_NODE_DIR : VFS_NODE_FILE, &node) != 0) {
            return errno_ret(ENOENT);
        }
    }
    if (!node) {
        return errno_ret(ENOENT);
    }
    if ((flags & O_TRUNC) && node->ops && node->ops->truncate) {
        node->ops->truncate(node, 0);
    }

    int fd = fd_alloc(node, (int)flags);
    if (fd >= 0 && (flags & O_APPEND)) {
        fd_entry_t *entry = fd_get(fd);
        if (entry) {
            entry->offset = (size_t)node->size;
        }
    }
    return fd < 0 ? errno_ret(ENOMEM) : fd;
}

long sys_close(uint64_t fd, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    return fd_close((int)fd) == 0 ? 0 : errno_ret(EBADF);
}

long sys_stat(uint64_t path_arg, uint64_t statbuf_arg, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;

    const char *path = (const char *)(uintptr_t)path_arg;
    user_stat_t *statbuf = (user_stat_t *)(uintptr_t)statbuf_arg;
    vfs_node_t *node = vfs_lookup(path);
    return node ? fill_stat(node, statbuf) : errno_ret(ENOENT);
}

long sys_fstat(uint64_t fd, uint64_t statbuf_arg, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;

    fd_entry_t *entry = fd_get((int)fd);
    return entry ? fill_stat(entry->node, (user_stat_t *)(uintptr_t)statbuf_arg) : errno_ret(EBADF);
}

long sys_seek(uint64_t fd, uint64_t offset, uint64_t whence,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused4;
    (void)unused5;
    (void)unused6;
    int result = fd_seek((int)fd, (int64_t)offset, (int)whence);
    return result < 0 ? errno_ret(EINVAL) : result;
}

long sys_brk(uint64_t address, uint64_t unused2, uint64_t unused3,
             uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    if (address != 0) {
        process_break = (uintptr_t)address;
    }
    return (long)process_break;
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

long sys_wait(uint64_t result_arg, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    process_t *current = process_current();
    wait_result_t result;
    int status = wait_any_child(current ? current->pid : 0, &result);
    if (status != 0) {
        return errno_ret(ENOENT);
    }
    if (result_arg) {
        *(int *)(uintptr_t)result_arg = result.exit_code;
    }
    return (long)result.pid;
}

long sys_waitpid(uint64_t pid, uint64_t status_arg, uint64_t options,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)options;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    wait_result_t result;
    if (wait_for_pid(pid, &result) != 0) {
        return errno_ret(ENOENT);
    }
    if (status_arg) {
        *(int *)(uintptr_t)status_arg = result.exit_code;
    }
    return (long)result.pid;
}

long sys_getpid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    process_t *current = process_current();
    return current ? (long)current->pid : 1;
}

long sys_getppid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    process_t *current = process_current();
    return current ? (long)current->parent_pid : 0;
}

long sys_getuid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    return 0;
}

long sys_getgid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    return sys_getuid(unused1, unused2, unused3, unused4, unused5, unused6);
}

long sys_dup(uint64_t fd, uint64_t unused2, uint64_t unused3,
             uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    int result = fd_dup((int)fd);
    return result < 0 ? errno_ret(EBADF) : result;
}

long sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    int result = fd_dup2((int)oldfd, (int)newfd);
    return result < 0 ? errno_ret(EBADF) : result;
}

long sys_pipe(uint64_t fds_arg, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    int *fds = (int *)(uintptr_t)fds_arg;
    if (!fds) {
        return errno_ret(EFAULT);
    }

    vfs_node_t *node = pipefs_create_node("pipe");
    if (!node) {
        return errno_ret(ENOMEM);
    }
    int read_fd = fd_alloc(node, 0);
    int write_fd = fd_alloc(node, 1);
    if (read_fd < 0 || write_fd < 0) {
        return errno_ret(ENOMEM);
    }
    fds[0] = read_fd;
    fds[1] = write_fd;
    return 0;
}

long sys_mkdir(uint64_t path_arg, uint64_t mode, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)mode;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    const char *path = (const char *)(uintptr_t)path_arg;
    make_parent_dirs(path);
    return vfs_create(path, VFS_NODE_DIR, 0) == 0 ? 0 : errno_ret(EINVAL);
}

long sys_unlink(uint64_t path, uint64_t unused2, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    return vfs_unlink((const char *)(uintptr_t)path) == 0 ? 0 : errno_ret(ENOENT);
}

long sys_chdir(uint64_t path_arg, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    const char *path = (const char *)(uintptr_t)path_arg;
    vfs_node_t *node = vfs_lookup(path);
    if (!node || node->type != VFS_NODE_DIR || strlen(path) >= sizeof(cwd)) {
        return errno_ret(ENOENT);
    }
    strcpy(cwd, path);
    return 0;
}

long sys_getcwd(uint64_t buffer_arg, uint64_t size, uint64_t unused3,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    char *buffer = (char *)(uintptr_t)buffer_arg;
    if (!buffer || size == 0 || strlen(cwd) + 1 > size) {
        return errno_ret(EFAULT);
    }
    strcpy(buffer, cwd);
    return (long)(uintptr_t)buffer;
}

long sys_opendir(uint64_t path_arg, uint64_t unused2, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    vfs_node_t *node = vfs_lookup((const char *)(uintptr_t)path_arg);
    if (!node || node->type != VFS_NODE_DIR) {
        return errno_ret(ENOENT);
    }
    int fd = fd_alloc(node, O_DIRECTORY);
    return fd < 0 ? errno_ret(ENOMEM) : fd;
}

long sys_readdir(uint64_t fd, uint64_t entry_arg, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    fd_entry_t *fd_entry = fd_get((int)fd);
    user_dirent_t *entry = (user_dirent_t *)(uintptr_t)entry_arg;
    if (!fd_entry || !entry || !fd_entry->node || fd_entry->node->type != VFS_NODE_DIR) {
        return errno_ret(EBADF);
    }

    vfs_node_t *child = fd_entry->node->children;
    for (size_t index = 0; child && index < fd_entry->offset; ++index) {
        child = child->next;
    }
    if (!child) {
        return 0;
    }

    memset(entry, 0, sizeof(*entry));
    entry->d_ino = (uint64_t)(uintptr_t)child;
    entry->d_type = dirent_type(child);
    size_t length = strlen(child->name);
    if (length >= sizeof(entry->d_name)) {
        length = sizeof(entry->d_name) - 1;
    }
    memcpy(entry->d_name, child->name, length);
    ++fd_entry->offset;
    return 1;
}

long sys_truncate(uint64_t path_arg, uint64_t length, uint64_t unused3,
                  uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    vfs_node_t *node = vfs_lookup((const char *)(uintptr_t)path_arg);
    if (!node || !node->ops || !node->ops->truncate) {
        return errno_ret(ENOENT);
    }
    return node->ops->truncate(node, (size_t)length);
}

long sys_ftruncate(uint64_t fd, uint64_t length, uint64_t unused3,
                   uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    fd_entry_t *entry = fd_get((int)fd);
    if (!entry || !entry->node || !entry->node->ops || !entry->node->ops->truncate) {
        return errno_ret(EBADF);
    }
    return entry->node->ops->truncate(entry->node, (size_t)length);
}

long sys_nanosleep(uint64_t req_arg, uint64_t rem, uint64_t unused3,
                   uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)rem;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    const user_timespec_t *req = (const user_timespec_t *)(uintptr_t)req_arg;
    if (!req) {
        return errno_ret(EFAULT);
    }
    uint64_t ms = (uint64_t)req->tv_sec * 1000ull + (uint64_t)req->tv_nsec / 1000000ull;
    timer_sleep_ms(ms);
    return 0;
}

long sys_kill(uint64_t pid, uint64_t signal, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    process_t *process = process_find(pid);
    return process && signal_send(&process->signals, (int)signal) ? 0 : errno_ret(ENOENT);
}

long sys_socket(uint64_t domain, uint64_t type, uint64_t protocol,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)domain;
    (void)protocol;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    int fd = socket_create(type == 1 ? SOCKET_TCP : SOCKET_UDP);
    return fd < 0 ? errno_ret(ENOMEM) : fd;
}

long sys_connect(uint64_t fd, uint64_t addr, uint64_t addrlen,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)addrlen;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    return socket_connect((int)fd, (uint32_t)addr, 0) == 0 ? 0 : errno_ret(EBADF);
}

long sys_shmget(uint64_t key, uint64_t size, uint64_t flags,
                uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)flags;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    char name[SHM_NAME_MAX];
    name[0] = 's';
    name[1] = 'h';
    name[2] = 'm';
    for (unsigned index = 0; index < 8 && 3 + index < SHM_NAME_MAX - 1; ++index) {
        unsigned nibble = (unsigned)((key >> (index * 4)) & 0xf);
        name[3 + index] = (char)(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
    }
    name[11] = '\0';
    shm_segment_t *segment = shm_create(name, (size_t)size);
    return segment ? (long)segment->id : errno_ret(ENOMEM);
}

long sys_shmat(uint64_t shmid, uint64_t addr, uint64_t flags,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)addr;
    (void)flags;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    shm_segment_t *segment = shm_find_id((uint32_t)shmid);
    void *base = shm_attach(segment);
    return base ? (long)(uintptr_t)base : errno_ret(ENOENT);
}

long sys_shmdt(uint64_t addr, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    for (uint32_t id = 1; id < 1024; ++id) {
        shm_segment_t *segment = shm_find_id(id);
        if (segment && segment->base == (void *)(uintptr_t)addr) {
            shm_detach(segment);
            return 0;
        }
    }
    return errno_ret(ENOENT);
}

long sys_uname(uint64_t uts_arg, uint64_t unused2, uint64_t unused3,
               uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
    user_utsname_t *uts = (user_utsname_t *)(uintptr_t)uts_arg;
    if (!uts) {
        return errno_ret(EFAULT);
    }
    memset(uts, 0, sizeof(*uts));
    strcpy(uts->sysname, "nv0ken_linux");
    strcpy(uts->nodename, "nv0ken");
    strcpy(uts->release, "0.1.0");
    strcpy(uts->version, "codex-kernel");
    strcpy(uts->machine, "x86_64");
    return 0;
}

long sys_sync(uint64_t unused1, uint64_t unused2, uint64_t unused3,
              uint64_t unused4, uint64_t unused5, uint64_t unused6)
{
    (void)unused1;
    (void)unused2;
    (void)unused3;
    (void)unused4;
    (void)unused5;
    (void)unused6;
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
