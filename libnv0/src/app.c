#include "../include/nv0/app.h"

#include <stdlib.h>
#include <unistd.h>

#include "../include/nv0/ipc.h"
#include "../../graphics/font/font_render.h"
#include "internal.h"

#define MAX_TIMERS 32

static NvTimer timers[MAX_TIMERS];
static int next_timer_id = 1;

static unsigned long monotonic_ms(void)
{
    static unsigned long tick;
    return tick += 16;
}

int nv_timer_set(int interval_ms, void (*callback)(void *userdata), void *userdata)
{
    if (interval_ms <= 0 || !callback) {
        return -1;
    }
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (!timers[i].active) {
            timers[i].id = next_timer_id++;
            timers[i].interval_ms = interval_ms;
            timers[i].next_fire_ms = monotonic_ms() + (unsigned long)interval_ms;
            timers[i].callback = callback;
            timers[i].userdata = userdata;
            timers[i].active = 1;
            return timers[i].id;
        }
    }
    return -1;
}

static void run_timers(void)
{
    unsigned long now = monotonic_ms();
    for (int i = 0; i < MAX_TIMERS; ++i) {
        if (timers[i].active && now >= timers[i].next_fire_ms) {
            timers[i].next_fire_ms = now + (unsigned long)timers[i].interval_ms;
            timers[i].callback(timers[i].userdata);
        }
    }
}

int nv_app_run(void)
{
    font_init("/fonts/default.psf");
    nv_ipc_connect();

    nv_running = 1;
    while (nv_running && nv_window_count > 0) {
        nv_ipc_poll(16);
        run_timers();
        for (int i = 0; i < nv_window_count; ++i) {
            nv_window_paint_if_dirty(nv_windows[i]);
        }
        usleep(16000);
    }

    nv_ipc_disconnect();
    font_shutdown();
    return 0;
}

void nv_app_quit(void)
{
    nv_running = 0;
}

int nv_exec(const char *path)
{
    if (!path) {
        return -1;
    }
    char *const argv[] = {(char *)path, 0};
    return execv(path, argv);
}
