#ifndef NV0_WINDOW_H
#define NV0_WINDOW_H

#include "draw.h"
#include "input.h"

typedef struct NvWindow NvWindow;

typedef struct {
    const char *title;
    int width;
    int height;
    int x;
    int y;
    int resizable;
    unsigned flags;
} NvWindowConfig;

typedef void (*NvPaintCallback)(NvWindow *window, NvSurface *surface, void *userdata);
typedef void (*NvMouseCallback)(NvWindow *window, NvMouseEvent *event, void *userdata);
typedef void (*NvKeyCallback)(NvWindow *window, NvKeyEvent *event, void *userdata);
typedef void (*NvScrollCallback)(NvWindow *window, NvScrollEvent *event, void *userdata);
typedef void (*NvCloseCallback)(NvWindow *window, void *userdata);

NvWindow *nv_window_create(const NvWindowConfig *config);
void      nv_window_destroy(NvWindow *window);
void      nv_window_set_title(NvWindow *window, const char *title);
void      nv_window_show(NvWindow *window);
void      nv_window_hide(NvWindow *window);
void      nv_window_close(NvWindow *window);
void      nv_close(NvWindow *window);

NvSurface *nv_window_surface(NvWindow *window);
int        nv_window_width(const NvWindow *window);
int        nv_window_height(const NvWindow *window);
int        nv_window_id(const NvWindow *window);

void nv_window_on_paint(NvWindow *window, NvPaintCallback cb, void *userdata);
void nv_window_on_mouse_down(NvWindow *window, NvMouseCallback cb, void *userdata);
void nv_window_on_mouse_up(NvWindow *window, NvMouseCallback cb, void *userdata);
void nv_window_on_mouse_move(NvWindow *window, NvMouseCallback cb, void *userdata);
void nv_window_on_key_down(NvWindow *window, NvKeyCallback cb, void *userdata);
void nv_window_on_key_up(NvWindow *window, NvKeyCallback cb, void *userdata);
void nv_window_on_scroll(NvWindow *window, NvScrollCallback cb, void *userdata);
void nv_window_on_close(NvWindow *window, NvCloseCallback cb, void *userdata);

#endif
