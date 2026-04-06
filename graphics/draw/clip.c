#include "clip.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */

void clip_init(ClipStack *cs, int surface_w, int surface_h) {
    if (!cs) return;
    memset(cs, 0, sizeof(ClipStack));
    cs->surface_w  = surface_w;
    cs->surface_h  = surface_h;
    cs->active.x   = 0;
    cs->active.y   = 0;
    cs->active.w   = surface_w;
    cs->active.h   = surface_h;
    cs->depth      = 0;
}

/* -----------------------------------------------------------------------
 * Stack management
 * --------------------------------------------------------------------- */

static void recompute_active(ClipStack *cs) {
    if (cs->depth == 0) {
        cs->active.x = 0;
        cs->active.y = 0;
        cs->active.w = cs->surface_w;
        cs->active.h = cs->surface_h;
        return;
    }
    cs->active = cs->stack[cs->depth - 1];
}

void clip_push(ClipStack *cs, const NvRect *r) {
    if (!cs || !r) return;

    NvRect clipped;
    if (cs->depth == 0) {
        clipped.x = r->x;
        clipped.y = r->y;
        clipped.w = r->w;
        clipped.h = r->h;
    } else {
        const NvRect *cur = &cs->active;
        int x1 = r->x > cur->x ? r->x : cur->x;
        int y1 = r->y > cur->y ? r->y : cur->y;
        int x2_r   = r->x + r->w;
        int y2_r   = r->y + r->h;
        int x2_cur = cur->x + cur->w;
        int y2_cur = cur->y + cur->h;
        int x2 = x2_r < x2_cur ? x2_r : x2_cur;
        int y2 = y2_r < y2_cur ? y2_r : y2_cur;
        clipped.x = x1;
        clipped.y = y1;
        clipped.w = x2 - x1;
        clipped.h = y2 - y1;
        if (clipped.w < 0) clipped.w = 0;
        if (clipped.h < 0) clipped.h = 0;
    }

    if (clipped.x < 0) {
        clipped.w += clipped.x;
        clipped.x  = 0;
        if (clipped.w < 0) clipped.w = 0;
    }
    if (clipped.y < 0) {
        clipped.h += clipped.y;
        clipped.y  = 0;
        if (clipped.h < 0) clipped.h = 0;
    }
    if (clipped.x + clipped.w > cs->surface_w)
        clipped.w = cs->surface_w - clipped.x;
    if (clipped.y + clipped.h > cs->surface_h)
        clipped.h = cs->surface_h - clipped.y;
    if (clipped.w < 0) clipped.w = 0;
    if (clipped.h < 0) clipped.h = 0;

    if (cs->depth < CLIP_STACK_DEPTH) {
        cs->stack[cs->depth++] = clipped;
    }
    recompute_active(cs);
}

void clip_pop(ClipStack *cs) {
    if (!cs || cs->depth == 0) return;
    cs->depth--;
    recompute_active(cs);
}

void clip_reset(ClipStack *cs) {
    if (!cs) return;
    cs->depth      = 0;
    cs->active.x   = 0;
    cs->active.y   = 0;
    cs->active.w   = cs->surface_w;
    cs->active.h   = cs->surface_h;
}

/* -----------------------------------------------------------------------
 * Query
 * --------------------------------------------------------------------- */

const NvRect *clip_active(const ClipStack *cs) {
    return cs ? &cs->active : NULL;
}

int clip_contains(const ClipStack *cs, int x, int y) {
    if (!cs) return 0;
    return x >= cs->active.x &&
           x <  cs->active.x + cs->active.w &&
           y >= cs->active.y &&
           y <  cs->active.y + cs->active.h;
}

int clip_rect_visible(const ClipStack *cs, const NvRect *r) {
    if (!cs || !r) return 0;
    if (cs->active.w <= 0 || cs->active.h <= 0) return 0;
    if (r->w <= 0 || r->h <= 0) return 0;
    return r->x < cs->active.x + cs->active.w &&
           r->x + r->w > cs->active.x &&
           r->y < cs->active.y + cs->active.h &&
           r->y + r->h > cs->active.y;
}

/* -----------------------------------------------------------------------
 * Clip primitives in-place
 * --------------------------------------------------------------------- */

int clip_pixel(ClipStack *cs, int *x, int *y) {
    return clip_contains(cs, *x, *y);
}

int clip_line_h(ClipStack *cs, int *x, int *y, int *w) {
    if (!cs || !x || !y || !w) return 0;
    if (*w <= 0) return 0;
    if (*y < cs->active.y || *y >= cs->active.y + cs->active.h) return 0;

    int x0 = cs->active.x;
    int x1 = cs->active.x + cs->active.w;

    if (*x < x0) { *w -= x0 - *x; *x = x0; }
    if (*x + *w > x1) *w = x1 - *x;
    return *w > 0;
}

int clip_line_v(ClipStack *cs, int *x, int *y, int *h) {
    if (!cs || !x || !y || !h) return 0;
    if (*h <= 0) return 0;
    if (*x < cs->active.x || *x >= cs->active.x + cs->active.w) return 0;

    int y0 = cs->active.y;
    int y1 = cs->active.y + cs->active.h;

    if (*y < y0) { *h -= y0 - *y; *y = y0; }
    if (*y + *h > y1) *h = y1 - *y;
    return *h > 0;
}

int clip_rect(ClipStack *cs, NvRect *r) {
    if (!cs || !r) return 0;
    if (r->w <= 0 || r->h <= 0) return 0;

    int ax  = cs->active.x,       ay  = cs->active.y;
    int ax2 = ax + cs->active.w,  ay2 = ay + cs->active.h;
    int rx2 = r->x + r->w,        ry2 = r->y + r->h;

    int nx  = r->x > ax  ? r->x  : ax;
    int ny  = r->y > ay  ? r->y  : ay;
    int nx2 = rx2  < ax2 ? rx2   : ax2;
    int ny2 = ry2  < ay2 ? ry2   : ay2;

    r->x = nx;
    r->y = ny;
    r->w = nx2 - nx;
    r->h = ny2 - ny;

    return r->w > 0 && r->h > 0;
}