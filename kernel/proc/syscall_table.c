#include "syscall_table.h"

#include "../arch/x86_64/syscall.h"
#include "syscall_handlers.h"

#define REGISTER(number, handler) syscall_register((number), (syscall_fn_t)(handler))

void syscall_table_init(void)
{
    syscall_init();
    REGISTER(SYS_READ, sys_read);
    REGISTER(SYS_WRITE, sys_write);
    REGISTER(SYS_OPEN, sys_open);
    REGISTER(SYS_CLOSE, sys_close);
    REGISTER(SYS_STAT, sys_stat);
    REGISTER(SYS_FSTAT, sys_fstat);
    REGISTER(SYS_LSTAT, sys_stat);
    REGISTER(SYS_SEEK, sys_seek);
    REGISTER(SYS_BRK, sys_brk);
    REGISTER(SYS_EXIT, sys_exit);
    REGISTER(SYS_WAIT, sys_wait);
    REGISTER(SYS_WAITPID, sys_waitpid);
    REGISTER(SYS_GETPID, sys_getpid);
    REGISTER(SYS_GETPPID, sys_getppid);
    REGISTER(SYS_GETUID, sys_getuid);
    REGISTER(SYS_GETGID, sys_getgid);
    REGISTER(SYS_DUP, sys_dup);
    REGISTER(SYS_DUP2, sys_dup2);
    REGISTER(SYS_PIPE, sys_pipe);
    REGISTER(SYS_MKDIR, sys_mkdir);
    REGISTER(SYS_RMDIR, sys_unlink);
    REGISTER(SYS_UNLINK, sys_unlink);
    REGISTER(SYS_CHDIR, sys_chdir);
    REGISTER(SYS_GETCWD, sys_getcwd);
    REGISTER(SYS_OPENDIR, sys_opendir);
    REGISTER(SYS_READDIR, sys_readdir);
    REGISTER(SYS_CLOSEDIR, sys_close);
    REGISTER(SYS_TRUNCATE, sys_truncate);
    REGISTER(SYS_FTRUNCATE, sys_ftruncate);
    REGISTER(SYS_NANOSLEEP, sys_nanosleep);
    REGISTER(SYS_TIME, sys_time);
    REGISTER(SYS_CLOCK_GETTIME, sys_clock_gettime);
    REGISTER(SYS_KILL, sys_kill);
    REGISTER(SYS_SOCKET, sys_socket);
    REGISTER(SYS_CONNECT, sys_connect);
    REGISTER(SYS_SHMGET, sys_shmget);
    REGISTER(SYS_SHMAT, sys_shmat);
    REGISTER(SYS_SHMDT, sys_shmdt);
    REGISTER(SYS_UNAME, sys_uname);
    REGISTER(SYS_SYNC, sys_sync);
    REGISTER(SYS_YIELD, sys_yield);
}
