#include "pty.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define READ_BUF_SIZE 4096

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

/* -----------------------------------------------------------------------
 * Platform backend
 * On nv0ken this calls into the kernel PTY syscalls.
 * On a POSIX host it uses openpty / fork.
 * --------------------------------------------------------------------- */

#ifdef NV0KEN_KERNEL
#include "../../../userland/libc/include/unistd.h"

struct Pty {
    int          master_fd;
    int          slave_fd;
    int          child_pid;
    int          cols;
    int          rows;
    PtyDataFn    on_data;
    void        *userdata;
    char         read_buf[READ_BUF_SIZE];
};

Pty *pty_open(int cols, int rows, PtyDataFn on_data, void *userdata) {
    Pty *p = xmalloc(sizeof(Pty));
    if (!p) return NULL;
    memset(p, 0, sizeof(Pty));

    p->cols     = cols;
    p->rows     = rows;
    p->on_data  = on_data;
    p->userdata = userdata;

    int fds[2];
    if (sys_pty_open(fds) < 0) { free(p); return NULL; }
    p->master_fd = fds[0];
    p->slave_fd  = fds[1];

    sys_pty_set_size(p->master_fd, cols, rows);

    p->child_pid = sys_fork();
    if (p->child_pid == 0) {
        sys_close(p->master_fd);
        sys_pty_make_controlling(p->slave_fd);
        sys_dup2(p->slave_fd, 0);
        sys_dup2(p->slave_fd, 1);
        sys_dup2(p->slave_fd, 2);
        sys_close(p->slave_fd);
        char *argv[] = { "/bin/nv0sh", NULL };
        char *envp[] = {
            "TERM=xterm-256color",
            "HOME=/home",
            "PATH=/bin:/usr/bin",
            "USER=user",
            NULL
        };
        sys_execve("/bin/nv0sh", argv, envp);
        sys_exit(1);
    }

    sys_close(p->slave_fd);
    p->slave_fd = -1;
    return p;
}

void pty_free(Pty *p) {
    if (!p) return;
    sys_close(p->master_fd);
    free(p);
}

int pty_write(Pty *p, const char *buf, int len) {
    if (!p || !buf || len <= 0) return -1;
    return sys_write(p->master_fd, buf, len);
}

int pty_poll(Pty *p) {
    if (!p) return 0;
    int n = sys_read_nonblock(p->master_fd, p->read_buf, READ_BUF_SIZE - 1);
    if (n > 0 && p->on_data) {
        p->read_buf[n] = '\0';
        p->on_data(p->read_buf, n, p->userdata);
    }
    return n > 0 ? n : 0;
}

int pty_resize(Pty *p, int cols, int rows) {
    if (!p) return -1;
    p->cols = cols;
    p->rows = rows;
    return sys_pty_set_size(p->master_fd, cols, rows);
}

int pty_master_fd(Pty *p) {
    return p ? p->master_fd : -1;
}

int pty_child_pid(Pty *p) {
    return p ? p->child_pid : -1;
}

#else

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>

#ifdef __linux__
#include <pty.h>
#elif defined(__APPLE__)
#include <util.h>
#endif

struct Pty {
    int          master_fd;
    int          child_pid;
    int          cols;
    int          rows;
    PtyDataFn    on_data;
    void        *userdata;
    char         read_buf[READ_BUF_SIZE];
};

Pty *pty_open(int cols, int rows, PtyDataFn on_data, void *userdata) {
    Pty *p = xmalloc(sizeof(Pty));
    if (!p) return NULL;
    memset(p, 0, sizeof(Pty));

    p->cols     = cols;
    p->rows     = rows;
    p->on_data  = on_data;
    p->userdata = userdata;

    struct winsize ws;
    ws.ws_col    = (unsigned short)cols;
    ws.ws_row    = (unsigned short)rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, &ws);

    if (pid < 0) { free(p); return NULL; }

    if (pid == 0) {
        char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/sh";
        char *argv[] = { shell, NULL };
        char *envp[] = {
            "TERM=xterm-256color",
            "HOME=/home",
            "PATH=/bin:/usr/bin:/usr/local/bin",
            "USER=user",
            NULL
        };
        execve(shell, argv, envp);
        _exit(1);
    }

    p->master_fd = master_fd;
    p->child_pid = (int)pid;

    int flags = fcntl(master_fd, F_GETFL, 0);
    fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

    return p;
}

void pty_free(Pty *p) {
    if (!p) return;
    if (p->child_pid > 0) {
        kill(p->child_pid, SIGHUP);
        waitpid(p->child_pid, NULL, WNOHANG);
    }
    close(p->master_fd);
    free(p);
}

int pty_write(Pty *p, const char *buf, int len) {
    if (!p || !buf || len <= 0) return -1;
    int total = 0;
    while (total < len) {
        int n = (int)write(p->master_fd, buf + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += n;
    }
    return total;
}

int pty_poll(Pty *p) {
    if (!p) return 0;
    int n = (int)read(p->master_fd, p->read_buf, READ_BUF_SIZE - 1);
    if (n > 0 && p->on_data) {
        p->read_buf[n] = '\0';
        p->on_data(p->read_buf, n, p->userdata);
    }
    return n > 0 ? n : 0;
}

int pty_resize(Pty *p, int cols, int rows) {
    if (!p) return -1;
    p->cols = cols;
    p->rows = rows;
    struct winsize ws;
    ws.ws_col    = (unsigned short)cols;
    ws.ws_row    = (unsigned short)rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    return ioctl(p->master_fd, TIOCSWINSZ, &ws);
}

int pty_master_fd(Pty *p) {
    return p ? p->master_fd : -1;
}

int pty_child_pid(Pty *p) {
    return p ? p->child_pid : -1;
}

#endif