bits 64

section .text

%macro SYSCALL0 2
global %1
%1:
    mov rax, %2
    syscall
    ret
%endmacro

%macro SYSCALL1 2
global %1
%1:
    mov rax, %2
    syscall
    ret
%endmacro

%macro SYSCALL2 2
global %1
%1:
    mov rax, %2
    syscall
    ret
%endmacro

%macro SYSCALL3 2
global %1
%1:
    mov rax, %2
    syscall
    ret
%endmacro

%macro SYSCALL4 2
global %1
%1:
    mov rax, %2
    mov r10, rcx
    syscall
    ret
%endmacro

%macro SYSCALL5 2
global %1
%1:
    mov rax, %2
    mov r10, rcx
    syscall
    ret
%endmacro

%macro SYSCALL6 2
global %1
%1:
    mov rax, %2
    mov r10, rcx
    syscall
    ret
%endmacro

SYSCALL3 sys_read,         0
SYSCALL3 sys_write,        1
SYSCALL3 sys_open,         2
SYSCALL1 sys_close,        3
SYSCALL2 sys_stat,         4
SYSCALL2 sys_fstat,        5
SYSCALL2 sys_lstat,        6
SYSCALL3 sys_seek,         7
SYSCALL6 sys_mmap,         8
SYSCALL2 sys_munmap,       9
SYSCALL1 sys_brk,          10
SYSCALL0 sys_fork,         11
SYSCALL3 sys_execve,       12
SYSCALL1 sys_exit,         13
SYSCALL1 sys_wait,         14
SYSCALL3 sys_waitpid,      15
SYSCALL0 sys_getpid,       16
SYSCALL0 sys_getppid,      17
SYSCALL0 sys_getuid,       18
SYSCALL0 sys_getgid,       19
SYSCALL1 sys_dup,          20
SYSCALL2 sys_dup2,         21
SYSCALL1 sys_pipe,         22
SYSCALL3 sys_fcntl,        23
SYSCALL3 sys_ioctl,        24
SYSCALL2 sys_mkdir,        25
SYSCALL1 sys_rmdir,        26
SYSCALL1 sys_unlink,       27
SYSCALL2 sys_rename,       28
SYSCALL1 sys_chdir,        29
SYSCALL2 sys_getcwd,       30
SYSCALL1 sys_opendir,      31
SYSCALL2 sys_readdir,      32
SYSCALL1 sys_closedir,     33
SYSCALL2 sys_symlink,      34
SYSCALL3 sys_readlink,     35
SYSCALL2 sys_chmod,        36
SYSCALL3 sys_chown,        37
SYSCALL3 sys_mount,        38
SYSCALL1 sys_umount,       39
SYSCALL2 sys_truncate,     40
SYSCALL2 sys_ftruncate,    41
SYSCALL2 sys_nanosleep,    42
SYSCALL1 sys_time,         43
SYSCALL2 sys_clock_gettime,44
SYSCALL2 sys_kill,         45
SYSCALL3 sys_sigaction,    46
SYSCALL3 sys_sigprocmask,  47
SYSCALL0 sys_sigreturn,    48
SYSCALL3 sys_socket,       49
SYSCALL3 sys_bind,         50
SYSCALL3 sys_connect,      51
SYSCALL2 sys_listen,       52
SYSCALL3 sys_accept,       53
SYSCALL4 sys_send,         54
SYSCALL4 sys_recv,         55
SYSCALL6 sys_sendto,       56
SYSCALL6 sys_recvfrom,     57
SYSCALL2 sys_shutdown,     58
SYSCALL5 sys_setsockopt,   59
SYSCALL5 sys_getsockopt,   60
SYSCALL1 sys_pty_open,     61
SYSCALL3 sys_pty_setsize,  62
SYSCALL3 sys_shmget,       63
SYSCALL3 sys_shmat,        64
SYSCALL1 sys_shmdt,        65
SYSCALL3 sys_shmctl,       66
SYSCALL3 sys_semget,       67
SYSCALL3 sys_semop,        68
SYSCALL4 sys_semctl,       69
SYSCALL2 sys_msgget,       70
SYSCALL4 sys_msgsnd,       71
SYSCALL5 sys_msgrcv,       72
SYSCALL3 sys_msgctl,       73
SYSCALL1 sys_uname,        74
SYSCALL1 sys_reboot,       75
SYSCALL0 sys_sync,         76