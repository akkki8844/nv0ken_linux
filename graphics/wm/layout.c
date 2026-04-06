#include "layout.h"
#include "wm.h"
#include <string.h>

static Window *visible_windows(WindowManager *wm, int *count) {
    static Window *ptrs[WM_MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < wm_window_count(wm); i++) {
        Window *w = wm_find_window(wm, wm->windows[i].id);
        if (w && w->visible &&
            w->state == WIN_STATE_NORMAL &&
            !(w->flags & WIN_FLAG_ALWAYS_TOP))
            ptrs[n++] = w;
    }
    if (count) *count = n;
    return ptrs[0] ? ptrs[0] : NULL;
}

void layout_tile_h(WindowManager *wm, int ax, int ay, int aw, int ah) {
    if (!wm) return;
    int count;
    Window *first = visible_windows(wm, &count);
    if (!count || !first) return;

    int each_w = aw / count;
    int x = ax;

    for (int i = 0; i < count; i++) {
        Window *w = &wm->windows[i];
        if (!w->visible || w->state != WIN_STATE_NORMAL) continue;
        int this_w = (i == count - 1) ? (ax + aw - x) : each_w;
        wm_move_window(wm, w->id, x, ay);
        wm_resize_window(wm, w->id, this_w, ah);
        x += this_w;
    }
}

void layout_tile_v(WindowManager *wm, int ax, int ay, int aw, int ah) {
    if (!wm) return;
    int count;
    visible_windows(wm, &count);
    if (!count) return;

    int each_h = ah / count;
    int y = ay;

    for (int i = 0; i < count; i++) {
        Window *w = &wm->windows[i];
        if (!w->visible || w->state != WIN_STATE_NORMAL) continue;
        int this_h = (i == count - 1) ? (ay + ah - y) : each_h;
        wm_move_window(wm, w->id, ax, y);
        wm_resize_window(wm, w->id, aw, this_h);
        y += this_h;
    }
}

void layout_monocle(WindowManager *wm, int ax, int ay, int aw, int ah) {
    if (!wm) return;
    for (int i = 0; i < wm_window_count(wm); i++) {
        Window *w = &wm->windows[i];
        if (!w->visible || w->state != WIN_STATE_NORMAL) continue;
        wm_move_window(wm, w->id, ax, ay);
        wm_resize_window(wm, w->id, aw, ah);
    }
}

void layout_apply(WindowManager *wm, LayoutMode mode,
                  int ax, int ay, int aw, int ah) {
    switch (mode) {
        case LAYOUT_TILE_H:  layout_tile_h(wm, ax, ay, aw, ah); break;
        case LAYOUT_TILE_V:  layout_tile_v(wm, ax, ay, aw, ah); break;
        case LAYOUT_MONOCLE: layout_monocle(wm, ax, ay, aw, ah); break;
        default: break;
    }
}