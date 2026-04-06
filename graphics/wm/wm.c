#include "wm.h"
#include "titlebar.h"
#include "window.h"
#include "../server/compositor.h"
#include "../server/ipc_server.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

struct WindowManager {
    Compositor *comp;
    int         screen_w;
    int         screen_h;
    int         taskbar_h;

    Window      windows[WM_MAX_WINDOWS];
    int         window_count;
    int         next_win_id;

    int         focused_id;
    int         drag_win_id;
    int         drag_mode;
    int         drag_start_mx;
    int         drag_start_my;
    int         drag_start_wx;
    int         drag_start_wy;
    int         drag_start_ww;
    int         drag_start_wh;

    int         snap_preview_x;
    int         snap_preview_y;
    int         snap_preview_w;
    int         snap_preview_h;
    int         snap_preview_active;
};

#define DRAG_NONE   0
#define DRAG_MOVE   1
#define DRAG_RESIZE 2

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

/* -----------------------------------------------------------------------
 * Layout helpers
 * --------------------------------------------------------------------- */

static void update_client_rect(Window *w) {
    if (w->flags & WIN_FLAG_DECORATED) {
        w->client_x = w->x + WM_BORDER_W;
        w->client_y = w->y + WM_TITLEBAR_H;
        w->client_w = w->w - WM_BORDER_W * 2;
        w->client_h = w->h - WM_TITLEBAR_H - WM_BORDER_W;
    } else {
        w->client_x = w->x;
        w->client_y = w->y;
        w->client_w = w->w;
        w->client_h = w->h;
    }
    if (w->client_w < 0) w->client_w = 0;
    if (w->client_h < 0) w->client_h = 0;
}

static void sort_windows_by_z(WindowManager *wm) {
    for (int i = 1; i < wm->window_count; i++) {
        Window tmp = wm->windows[i];
        int j = i - 1;
        while (j >= 0 && wm->windows[j].z_order > tmp.z_order) {
            wm->windows[j+1] = wm->windows[j];
            j--;
        }
        wm->windows[j+1] = tmp;
    }
}

static int next_cascade_x = 60;
static int next_cascade_y = 60;

static void cascade_position(WindowManager *wm, int *x, int *y) {
    *x = next_cascade_x;
    *y = next_cascade_y;
    next_cascade_x += 28;
    next_cascade_y += 28;
    if (next_cascade_x > wm->screen_w / 2) { next_cascade_x = 60; next_cascade_y = 60; }
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

WindowManager *wm_new(Compositor *comp, int screen_w, int screen_h) {
    WindowManager *wm = xmalloc(sizeof(WindowManager));
    if (!wm) return NULL;
    wm->comp       = comp;
    wm->screen_w   = screen_w;
    wm->screen_h   = screen_h;
    wm->taskbar_h  = 36;
    wm->next_win_id = 1;
    wm->focused_id  = -1;
    wm->drag_win_id = -1;
    return wm;
}

void wm_free(WindowManager *wm) {
    if (!wm) return;
    for (int i = 0; i < wm->window_count; i++) {
        Window *w = &wm->windows[i];
        if (wm->comp) {
            if (w->comp_surface_id > 0)
                compositor_destroy_surface(wm->comp, w->comp_surface_id);
            if (w->frame_surface_id > 0)
                compositor_destroy_surface(wm->comp, w->frame_surface_id);
        }
    }
    free(wm);
}

/* -----------------------------------------------------------------------
 * Window creation
 * --------------------------------------------------------------------- */

Window *wm_create_window(WindowManager *wm, IpcClient *owner,
                          int x, int y, int w, int h,
                          const char *title, int flags) {
    if (!wm || wm->window_count >= WM_MAX_WINDOWS) return NULL;
    if (w < WM_MIN_WIDTH)  w = WM_MIN_WIDTH;
    if (h < WM_MIN_HEIGHT) h = WM_MIN_HEIGHT;

    if (x < 0 && y < 0) cascade_position(wm, &x, &y);
    if (x + w > wm->screen_w) x = wm->screen_w - w - WM_BORDER_W;
    if (y + h > wm->screen_h - wm->taskbar_h)
        y = wm->screen_h - wm->taskbar_h - h - WM_BORDER_W;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    Window *win = &wm->windows[wm->window_count++];
    memset(win, 0, sizeof(Window));

    win->id      = wm->next_win_id++;
    win->x       = x;
    win->y       = y;
    win->w       = w;
    win->h       = h;
    win->state   = WIN_STATE_NORMAL;
    win->flags   = (flags > 0) ? flags : WIN_FLAG_DECORATED | WIN_FLAG_RESIZABLE;
    win->visible = 1;
    win->owner   = owner;
    win->z_order = 100 + wm->window_count;

    if (title) strncpy(win->title, title, sizeof(win->title) - 1);

    update_client_rect(win);

    if (wm->comp) {
        win->frame_surface_id = compositor_create_surface(
            wm->comp, x, y, w, h, SURFACE_NORMAL);
        win->comp_surface_id = compositor_create_surface(
            wm->comp,
            win->client_x, win->client_y,
            win->client_w, win->client_h,
            SURFACE_NORMAL);
        compositor_raise_surface(wm->comp, win->frame_surface_id);
    }

    wm_focus_window(wm, win->id);
    sort_windows_by_z(wm);
    return win;
}

/* -----------------------------------------------------------------------
 * Window destruction
 * --------------------------------------------------------------------- */

void wm_destroy_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    for (int i = 0; i < wm->window_count; i++) {
        Window *w = &wm->windows[i];
        if (w->id != win_id) continue;

        if (wm->comp) {
            if (w->comp_surface_id > 0)
                compositor_destroy_surface(wm->comp, w->comp_surface_id);
            if (w->frame_surface_id > 0)
                compositor_destroy_surface(wm->comp, w->frame_surface_id);
        }

        memmove(&wm->windows[i], &wm->windows[i+1],
                sizeof(Window) * (wm->window_count - i - 1));
        wm->window_count--;

        if (wm->focused_id == win_id) {
            wm->focused_id = -1;
            if (wm->window_count > 0)
                wm_focus_window(wm, wm->windows[wm->window_count-1].id);
        }
        return;
    }
}

Window *wm_find_window(WindowManager *wm, int win_id) {
    if (!wm) return NULL;
    for (int i = 0; i < wm->window_count; i++)
        if (wm->windows[i].id == win_id) return &wm->windows[i];
    return NULL;
}

/* -----------------------------------------------------------------------
 * State changes
 * --------------------------------------------------------------------- */

void wm_focus_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w || !w->visible) return;

    for (int i = 0; i < wm->window_count; i++) {
        if (wm->windows[i].focused && wm->windows[i].id != win_id) {
            wm->windows[i].focused = 0;
            if (wm->comp && wm->windows[i].owner)
                ipc_send_focus(wm->windows[i].owner,
                                wm->windows[i].id, 0);
            compositor_damage_surface(wm->comp,
                                      wm->windows[i].frame_surface_id);
        }
    }

    w->focused = 1;
    wm->focused_id = win_id;
    compositor_raise_surface(wm->comp, w->frame_surface_id);
    compositor_raise_surface(wm->comp, w->comp_surface_id);

    if (wm->comp && w->owner)
        ipc_send_focus(w->owner, w->id, 1);
    compositor_damage_surface(wm->comp, w->frame_surface_id);
    sort_windows_by_z(wm);
}

void wm_minimize_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    w->state   = WIN_STATE_MINIMIZED;
    w->visible = 0;
    if (wm->comp) {
        compositor_hide_surface(wm->comp, w->frame_surface_id);
        compositor_hide_surface(wm->comp, w->comp_surface_id);
    }
    if (wm->focused_id == win_id) {
        wm->focused_id = -1;
        for (int i = wm->window_count - 1; i >= 0; i--) {
            if (wm->windows[i].visible && wm->windows[i].id != win_id) {
                wm_focus_window(wm, wm->windows[i].id);
                break;
            }
        }
    }
}

void wm_restore_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;

    if (w->state == WIN_STATE_MAXIMIZED || w->state == WIN_STATE_FULLSCREEN) {
        w->x = w->restore_x; w->y = w->restore_y;
        w->w = w->restore_w; w->h = w->restore_h;
        update_client_rect(w);
        if (wm->comp) {
            compositor_move_surface(wm->comp, w->frame_surface_id, w->x, w->y);
            compositor_resize_surface(wm->comp, w->frame_surface_id, w->w, w->h);
            compositor_move_surface(wm->comp, w->comp_surface_id,
                                     w->client_x, w->client_y);
            compositor_resize_surface(wm->comp, w->comp_surface_id,
                                       w->client_w, w->client_h);
        }
    }

    w->state   = WIN_STATE_NORMAL;
    w->visible = 1;
    if (wm->comp) {
        compositor_show_surface(wm->comp, w->frame_surface_id);
        compositor_show_surface(wm->comp, w->comp_surface_id);
    }
    wm_focus_window(wm, win_id);
}

void wm_maximize_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;

    if (w->state == WIN_STATE_MAXIMIZED) { wm_restore_window(wm, win_id); return; }

    w->restore_x = w->x; w->restore_y = w->y;
    w->restore_w = w->w; w->restore_h = w->h;
    w->x = 0;
    w->y = 0;
    w->w = wm->screen_w;
    w->h = wm->screen_h - wm->taskbar_h;
    w->state = WIN_STATE_MAXIMIZED;
    update_client_rect(w);

    if (wm->comp) {
        compositor_move_surface(wm->comp, w->frame_surface_id, w->x, w->y);
        compositor_resize_surface(wm->comp, w->frame_surface_id, w->w, w->h);
        compositor_move_surface(wm->comp, w->comp_surface_id, w->client_x, w->client_y);
        compositor_resize_surface(wm->comp, w->comp_surface_id, w->client_w, w->client_h);
        if (w->owner) ipc_send_resize_notify(w->owner, w->id, w->client_w, w->client_h);
    }
}

void wm_fullscreen_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    if (w->state == WIN_STATE_FULLSCREEN) { wm_restore_window(wm, win_id); return; }

    w->restore_x = w->x; w->restore_y = w->y;
    w->restore_w = w->w; w->restore_h = w->h;
    w->x = 0; w->y = 0;
    w->w = wm->screen_w; w->h = wm->screen_h;
    w->state = WIN_STATE_FULLSCREEN;
    update_client_rect(w);

    if (wm->comp) {
        compositor_move_surface(wm->comp, w->frame_surface_id, 0, 0);
        compositor_resize_surface(wm->comp, w->frame_surface_id,
                                   wm->screen_w, wm->screen_h);
        compositor_move_surface(wm->comp, w->comp_surface_id, 0, 0);
        compositor_resize_surface(wm->comp, w->comp_surface_id,
                                   wm->screen_w, wm->screen_h);
        if (w->owner) ipc_send_resize_notify(w->owner, w->id,
                                              wm->screen_w, wm->screen_h);
    }
}

void wm_close_window(WindowManager *wm, int win_id) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    if (w->owner)
        ipc_send_close_request(w->owner, win_id);
    else
        wm_destroy_window(wm, win_id);
}

void wm_set_title(WindowManager *wm, int win_id, const char *title) {
    Window *w = wm_find_window(wm, win_id);
    if (!w || !title) return;
    strncpy(w->title, title, sizeof(w->title) - 1);
    if (wm->comp) compositor_damage_surface(wm->comp, w->frame_surface_id);
}

void wm_resize_window(WindowManager *wm, int win_id, int nw, int nh) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    if (nw < WM_MIN_WIDTH)  nw = WM_MIN_WIDTH;
    if (nh < WM_MIN_HEIGHT) nh = WM_MIN_HEIGHT;
    w->w = nw; w->h = nh;
    update_client_rect(w);
    if (wm->comp) {
        compositor_resize_surface(wm->comp, w->frame_surface_id, w->w, w->h);
        compositor_move_surface(wm->comp, w->comp_surface_id,
                                 w->client_x, w->client_y);
        compositor_resize_surface(wm->comp, w->comp_surface_id,
                                   w->client_w, w->client_h);
        if (w->owner) ipc_send_resize_notify(w->owner, w->id,
                                              w->client_w, w->client_h);
    }
}

void wm_move_window(WindowManager *wm, int win_id, int x, int y) {
    if (!wm) return;
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    w->x = x; w->y = y;
    update_client_rect(w);
    if (wm->comp) {
        compositor_move_surface(wm->comp, w->frame_surface_id, x, y);
        compositor_move_surface(wm->comp, w->comp_surface_id,
                                 w->client_x, w->client_y);
    }
}

void wm_show_window(WindowManager *wm, int win_id) {
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    w->visible = 1;
    if (wm->comp) {
        compositor_show_surface(wm->comp, w->frame_surface_id);
        compositor_show_surface(wm->comp, w->comp_surface_id);
    }
    wm_focus_window(wm, win_id);
}

void wm_hide_window(WindowManager *wm, int win_id) {
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    w->visible = 0;
    if (wm->comp) {
        compositor_hide_surface(wm->comp, w->frame_surface_id);
        compositor_hide_surface(wm->comp, w->comp_surface_id);
    }
}

void wm_raise_window(WindowManager *wm, int win_id) {
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    int max_z = 0;
    for (int i = 0; i < wm->window_count; i++)
        if (wm->windows[i].z_order > max_z) max_z = wm->windows[i].z_order;
    w->z_order = max_z + 1;
    sort_windows_by_z(wm);
    if (wm->comp) compositor_raise_surface(wm->comp, w->frame_surface_id);
}

void wm_lower_window(WindowManager *wm, int win_id) {
    Window *w = wm_find_window(wm, win_id);
    if (!w) return;
    if (w->z_order > 0) w->z_order--;
    sort_windows_by_z(wm);
}

/* -----------------------------------------------------------------------
 * Hit testing
 * --------------------------------------------------------------------- */

HitZone wm_hit_test(WindowManager *wm, int x, int y, int *out_win_id) {
    if (!wm) return HIT_NONE;
    if (out_win_id) *out_win_id = -1;

    for (int i = wm->window_count - 1; i >= 0; i--) {
        Window *w = &wm->windows[i];
        if (!w->visible || w->state == WIN_STATE_MINIMIZED) continue;

        if (x < w->x || x >= w->x + w->w ||
            y < w->y || y >= w->y + w->h) continue;

        if (out_win_id) *out_win_id = w->id;

        if (!(w->flags & WIN_FLAG_DECORATED))
            return HIT_CLIENT;

        /* Close / minimize / maximize buttons */
        if (y >= w->y && y < w->y + WM_TITLEBAR_H) {
            if (x >= w->x + w->w - 24 && x < w->x + w->w - 4)
                return HIT_CLOSE;
            if (x >= w->x + w->w - 48 && x < w->x + w->w - 28)
                return HIT_MAXIMIZE;
            if (x >= w->x + w->w - 72 && x < w->x + w->w - 52)
                return HIT_MINIMIZE;
            return HIT_TITLEBAR;
        }

        if (!(w->flags & WIN_FLAG_RESIZABLE))
            return HIT_CLIENT;

        int r = WM_RESIZE_HANDLE;
        int right  = x >= w->x + w->w - r;
        int bottom = y >= w->y + w->h - r;
        int left   = x < w->x + r;
        int top    = y < w->y + r;

        if (top    && left)  return HIT_RESIZE_NW;
        if (top    && right) return HIT_RESIZE_NE;
        if (bottom && left)  return HIT_RESIZE_SW;
        if (bottom && right) return HIT_RESIZE_SE;
        if (top)    return HIT_RESIZE_N;
        if (bottom) return HIT_RESIZE_S;
        if (left)   return HIT_RESIZE_W;
        if (right)  return HIT_RESIZE_E;

        return HIT_CLIENT;
    }
    return HIT_NONE;
}

/* -----------------------------------------------------------------------
 * Mouse input
 * --------------------------------------------------------------------- */

void wm_mouse_down(WindowManager *wm, int x, int y, int button,
                   uint32_t mods) {
    if (!wm) return;
    int win_id;
    HitZone hz = wm_hit_test(wm, x, y, &win_id);

    if (win_id > 0 && win_id != wm->focused_id)
        wm_focus_window(wm, win_id);

    Window *w = win_id > 0 ? wm_find_window(wm, win_id) : NULL;

    switch (hz) {
        case HIT_CLOSE:
            if (w) wm_close_window(wm, win_id);
            return;
        case HIT_MAXIMIZE:
            if (w) wm_maximize_window(wm, win_id);
            return;
        case HIT_MINIMIZE:
            if (w) wm_minimize_window(wm, win_id);
            return;
        case HIT_TITLEBAR:
            if (!w) return;
            wm->drag_win_id    = win_id;
            wm->drag_mode      = DRAG_MOVE;
            wm->drag_start_mx  = x;
            wm->drag_start_my  = y;
            wm->drag_start_wx  = w->x;
            wm->drag_start_wy  = w->y;
            return;
        case HIT_RESIZE_N: case HIT_RESIZE_S: case HIT_RESIZE_E:
        case HIT_RESIZE_W: case HIT_RESIZE_NE: case HIT_RESIZE_NW:
        case HIT_RESIZE_SE: case HIT_RESIZE_SW:
            if (!w) return;
            wm->drag_win_id   = win_id;
            wm->drag_mode     = DRAG_RESIZE + (int)hz;
            wm->drag_start_mx = x;
            wm->drag_start_my = y;
            wm->drag_start_wx = w->x;
            wm->drag_start_wy = w->y;
            wm->drag_start_ww = w->w;
            wm->drag_start_wh = w->h;
            return;
        case HIT_CLIENT:
            if (w && w->owner)
                ipc_send_mouse(w->owner, w->id, NV_MSG_MOUSE_DOWN,
                               x - w->client_x, y - w->client_y,
                               button, mods);
            return;
        default:
            return;
    }
}

void wm_mouse_up(WindowManager *wm, int x, int y, int button,
                 uint32_t mods) {
    if (!wm) return;

    if (wm->drag_mode == DRAG_MOVE && wm->drag_win_id > 0) {
        Window *w = wm_find_window(wm, wm->drag_win_id);
        if (w && wm->snap_preview_active) {
            wm_move_window(wm, w->id,
                           wm->snap_preview_x, wm->snap_preview_y);
            if (wm->snap_preview_w != w->w || wm->snap_preview_h != w->h)
                wm_resize_window(wm, w->id,
                                 wm->snap_preview_w, wm->snap_preview_h);
            wm->snap_preview_active = 0;
        }
    }

    wm->drag_win_id = -1;
    wm->drag_mode   = DRAG_NONE;
    wm->snap_preview_active = 0;

    int win_id;
    HitZone hz = wm_hit_test(wm, x, y, &win_id);
    if (hz == HIT_CLIENT) {
        Window *w = wm_find_window(wm, win_id);
        if (w && w->owner)
            ipc_send_mouse(w->owner, w->id, NV_MSG_MOUSE_UP,
                           x - w->client_x, y - w->client_y,
                           button, mods);
    }
}

void wm_mouse_move(WindowManager *wm, int x, int y, uint32_t mods) {
    if (!wm) return;

    if (wm->drag_win_id > 0 && wm->drag_mode == DRAG_MOVE) {
        Window *w = wm_find_window(wm, wm->drag_win_id);
        if (!w) goto pass_through;

        if (w->state == WIN_STATE_MAXIMIZED)
            wm_restore_window(wm, wm->drag_win_id);

        int nx = wm->drag_start_wx + (x - wm->drag_start_mx);
        int ny = wm->drag_start_wy + (y - wm->drag_start_my);

        /* Screen edge snapping */
        wm->snap_preview_active = 0;
        int snapped = 0;

        if (x <= WM_SNAP_THRESHOLD) {
            wm->snap_preview_x = 0;
            wm->snap_preview_y = 0;
            wm->snap_preview_w = wm->screen_w / 2;
            wm->snap_preview_h = wm->screen_h - wm->taskbar_h;
            wm->snap_preview_active = 1;
            snapped = 1;
        } else if (x >= wm->screen_w - WM_SNAP_THRESHOLD) {
            wm->snap_preview_x = wm->screen_w / 2;
            wm->snap_preview_y = 0;
            wm->snap_preview_w = wm->screen_w / 2;
            wm->snap_preview_h = wm->screen_h - wm->taskbar_h;
            wm->snap_preview_active = 1;
            snapped = 1;
        } else if (y <= WM_SNAP_THRESHOLD) {
            wm_maximize_window(wm, wm->drag_win_id);
            wm->drag_win_id = -1;
            return;
        }

        if (!snapped) {
            if (ny < 0) ny = 0;
            if (ny + WM_TITLEBAR_H > wm->screen_h - wm->taskbar_h)
                ny = wm->screen_h - wm->taskbar_h - WM_TITLEBAR_H;
            wm_move_window(wm, wm->drag_win_id, nx, ny);
        }
        return;
    }

    if (wm->drag_win_id > 0 && wm->drag_mode >= DRAG_RESIZE) {
        Window *w = wm_find_window(wm, wm->drag_win_id);
        if (!w) goto pass_through;

        int dx = x - wm->drag_start_mx;
        int dy = y - wm->drag_start_my;
        int hit = wm->drag_mode - DRAG_RESIZE;
        int nx = w->x, ny = w->y, nw = w->w, nh = w->h;

        if (hit == HIT_RESIZE_E  || hit == HIT_RESIZE_NE || hit == HIT_RESIZE_SE)
            nw = wm->drag_start_ww + dx;
        if (hit == HIT_RESIZE_W  || hit == HIT_RESIZE_NW || hit == HIT_RESIZE_SW) {
            nw = wm->drag_start_ww - dx;
            nx = wm->drag_start_wx + dx;
        }
        if (hit == HIT_RESIZE_S  || hit == HIT_RESIZE_SE || hit == HIT_RESIZE_SW)
            nh = wm->drag_start_wh + dy;
        if (hit == HIT_RESIZE_N  || hit == HIT_RESIZE_NE || hit == HIT_RESIZE_NW) {
            nh = wm->drag_start_wh - dy;
            ny = wm->drag_start_wy + dy;
        }

        if (nw < WM_MIN_WIDTH)  { nw = WM_MIN_WIDTH;  if (hit == HIT_RESIZE_W || hit == HIT_RESIZE_NW || hit == HIT_RESIZE_SW) nx = w->x + w->w - WM_MIN_WIDTH; }
        if (nh < WM_MIN_HEIGHT) { nh = WM_MIN_HEIGHT; if (hit == HIT_RESIZE_N || hit == HIT_RESIZE_NE || hit == HIT_RESIZE_NW) ny = w->y + w->h - WM_MIN_HEIGHT; }

        wm_move_window(wm, wm->drag_win_id, nx, ny);
        wm_resize_window(wm, wm->drag_win_id, nw, nh);
        return;
    }

pass_through:;
    int win_id;
    HitZone hz = wm_hit_test(wm, x, y, &win_id);
    if (hz == HIT_CLIENT) {
        Window *w = wm_find_window(wm, win_id);
        if (w && w->owner)
            ipc_send_mouse(w->owner, w->id, NV_MSG_MOUSE_MOVE,
                           x - w->client_x, y - w->client_y,
                           0, mods);
    }
}

/* -----------------------------------------------------------------------
 * Keyboard input — route to focused window
 * --------------------------------------------------------------------- */

void wm_key_down(WindowManager *wm, uint32_t key, uint32_t cp, uint32_t mods) {
    if (!wm || wm->focused_id < 0) return;
    Window *w = wm_find_window(wm, wm->focused_id);
    if (!w || !w->owner) return;
    ipc_send_key(w->owner, w->id, NV_MSG_KEY_DOWN, key, cp, mods);
}

void wm_key_up(WindowManager *wm, uint32_t key, uint32_t cp, uint32_t mods) {
    if (!wm || wm->focused_id < 0) return;
    Window *w = wm_find_window(wm, wm->focused_id);
    if (!w || !w->owner) return;
    ipc_send_key(w->owner, w->id, NV_MSG_KEY_UP, key, cp, mods);
}

/* -----------------------------------------------------------------------
 * Draw all window frames
 * --------------------------------------------------------------------- */

void wm_draw_all(WindowManager *wm, NvSurface *surface) {
    if (!wm || !surface) return;
    for (int i = 0; i < wm->window_count; i++) {
        Window *w = &wm->windows[i];
        if (!w->visible || w->state == WIN_STATE_MINIMIZED) continue;
        NvSurface *frame_buf = compositor_surface_buffer(
            wm->comp, w->frame_surface_id);
        if (frame_buf) {
            titlebar_draw(frame_buf, w->w, w->h,
                          w->title, w->focused,
                          w->state == WIN_STATE_MAXIMIZED,
                          w->flags);
            compositor_damage_surface(wm->comp, w->frame_surface_id);
        }
    }

    /* Draw snap preview overlay */
    if (wm->snap_preview_active) {
        NvRect r = { wm->snap_preview_x, wm->snap_preview_y,
                     wm->snap_preview_w, wm->snap_preview_h };
        draw_fill_rect_alpha(surface, &r, 0x0078D730);
        draw_rect(surface, &r, 0x0078D7CC);
    }
}

int wm_window_count(WindowManager *wm) { return wm ? wm->window_count : 0; }
int wm_focused_window(WindowManager *wm) { return wm ? wm->focused_id : -1; }

Window *wm_get_window_at(WindowManager *wm, int x, int y) {
    int id;
    HitZone hz = wm_hit_test(wm, x, y, &id);
    (void)hz;
    return id > 0 ? wm_find_window(wm, id) : NULL;
}