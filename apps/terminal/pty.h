#ifndef PTY_H
#define PTY_H

typedef void (*PtyDataFn)(const char *buf, int len, void *userdata);

typedef struct Pty Pty;

Pty *pty_open(int cols, int rows, PtyDataFn on_data, void *userdata);
void pty_free(Pty *p);
int  pty_write(Pty *p, const char *buf, int len);
int  pty_poll(Pty *p);
int  pty_resize(Pty *p, int cols, int rows);
int  pty_master_fd(Pty *p);
int  pty_child_pid(Pty *p);

#endif