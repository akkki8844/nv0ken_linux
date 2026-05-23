#ifndef NV0_INTERNAL_H
#define NV0_INTERNAL_H

#include "../include/nv0/window.h"
#include "../../graphics/server/ipc_server.h"

#define NV0_MAX_WINDOWS 32

typedef struct NvTimer {
    int id;
    int interval_ms;
    unsigned long next_fire_ms;
    void (*callback)(void *userdata);
    void *userdata;
    int active;
} NvTimer;

struct NvWindow {
    int id;
    int width;
    int height;
    int x;
    int y;
    int visible;
    int dirty;
    char title[128];
    NvSurface *surface;
    NvPaintCallback on_paint;
    NvMouseCallback on_mouse_down;
    NvMouseCallback on_mouse_up;
    NvMouseCallback on_mouse_move;
    NvKeyCallback on_key_down;
    NvKeyCallback on_key_up;
    NvScrollCallback on_scroll;
    NvCloseCallback on_close;
    void *paint_userdata;
    void *mouse_down_userdata;
    void *mouse_up_userdata;
    void *mouse_move_userdata;
    void *key_down_userdata;
    void *key_up_userdata;
    void *scroll_userdata;
    void *close_userdata;
};

extern NvWindow *nv_windows[NV0_MAX_WINDOWS];
extern int nv_window_count;
extern int nv_running;

NvWindow *nv_window_find(int id);
void nv_window_mark_dirty(NvWindow *window);
void nv_window_paint_if_dirty(NvWindow *window);
int nv_window_dispatch(uint32_t type, const void *payload, uint32_t length);
int nv_window_flush(NvWindow *window);

#endif
