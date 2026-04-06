#include "resize.h"
#include <string.h>

static int snap(int v, int grid) {
    if (grid <= 1) return v;
    return (v / grid) * grid;
}

void resize_begin(ResizeState *rs, int win_id, HitZone edge,
                  int mx, int my, int wx, int wy,
                  int ww, int wh, int min_w, int min_h) {
    if (!rs) return;
    memset(rs, 0, sizeof(ResizeState));
    rs->win_id    = win_id;
    rs->edge      = edge;
    rs->active    = 1;
    rs->start_mx  = mx;
    rs->start_my  = my;
    rs->start_wx  = wx;
    rs->start_wy  = wy;
    rs->start_ww  = ww;
    rs->start_wh  = wh;
    rs->min_w     = min_w > 0 ? min_w : WM_MIN_WIDTH;
    rs->min_h     = min_h > 0 ? min_h : WM_MIN_HEIGHT;
    rs->max_w     = 8192;
    rs->max_h     = 8192;
    rs->grid_snap = 1;
}

void resize_update(ResizeState *rs, int mx, int my,
                   int *out_x, int *out_y, int *out_w, int *out_h) {
    if (!rs || !rs->active) {
        if (out_x) *out_x = 0; if (out_y) *out_y = 0;
        if (out_w) *out_w = 0; if (out_h) *out_h = 0;
        return;
    }

    int dx = mx - rs->start_mx;
    int dy = my - rs->start_my;
    int nx = rs->start_wx, ny = rs->start_wy;
    int nw = rs->start_ww, nh = rs->start_wh;

    switch (rs->edge) {
        case HIT_RESIZE_E:
            nw = rs->start_ww + dx;
            break;
        case HIT_RESIZE_W:
            nw = rs->start_ww - dx;
            nx = rs->start_wx + dx;
            break;
        case HIT_RESIZE_S:
            nh = rs->start_wh + dy;
            break;
        case HIT_RESIZE_N:
            nh = rs->start_wh - dy;
            ny = rs->start_wy + dy;
            break;
        case HIT_RESIZE_SE:
            nw = rs->start_ww + dx;
            nh = rs->start_wh + dy;
            break;
        case HIT_RESIZE_SW:
            nw = rs->start_ww - dx;
            nh = rs->start_wh + dy;
            nx = rs->start_wx + dx;
            break;
        case HIT_RESIZE_NE:
            nw = rs->start_ww + dx;
            nh = rs->start_wh - dy;
            ny = rs->start_wy + dy;
            break;
        case HIT_RESIZE_NW:
            nw = rs->start_ww - dx;
            nh = rs->start_wh - dy;
            nx = rs->start_wx + dx;
            ny = rs->start_wy + dy;
            break;
        default:
            break;
    }

    /* Clamp to min/max */
    if (nw < rs->min_w) {
        if (rs->edge == HIT_RESIZE_W || rs->edge == HIT_RESIZE_NW ||
            rs->edge == HIT_RESIZE_SW)
            nx = rs->start_wx + rs->start_ww - rs->min_w;
        nw = rs->min_w;
    }
    if (nw > rs->max_w) nw = rs->max_w;
    if (nh < rs->min_h) {
        if (rs->edge == HIT_RESIZE_N || rs->edge == HIT_RESIZE_NW ||
            rs->edge == HIT_RESIZE_NE)
            ny = rs->start_wy + rs->start_wh - rs->min_h;
        nh = rs->min_h;
    }
    if (nh > rs->max_h) nh = rs->max_h;

    /* Grid snap */
    nw = snap(nw, rs->grid_snap);
    nh = snap(nh, rs->grid_snap);

    if (out_x) *out_x = nx;
    if (out_y) *out_y = ny;
    if (out_w) *out_w = nw;
    if (out_h) *out_h = nh;
}

void resize_end(ResizeState *rs) {
    if (rs) rs->active = 0;
}

int resize_active(const ResizeState *rs) {
    return rs ? rs->active : 0;
}

void resize_set_constraints(ResizeState *rs, int min_w, int min_h,
                             int max_w, int max_h) {
    if (!rs) return;
    if (min_w > 0) rs->min_w = min_w;
    if (min_h > 0) rs->min_h = min_h;
    if (max_w > 0) rs->max_w = max_w;
    if (max_h > 0) rs->max_h = max_h;
}

void resize_set_grid(ResizeState *rs, int grid) {
    if (rs) rs->grid_snap = grid > 0 ? grid : 1;
}