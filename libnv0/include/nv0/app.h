#ifndef NV0_APP_H
#define NV0_APP_H

#include "fs.h"

int  nv_app_run(void);
void nv_app_quit(void);
int  nv_timer_set(int interval_ms, void (*callback)(void *userdata), void *userdata);
int  nv_exec(const char *path);

#endif
