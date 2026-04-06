#include "damage.h"
#include <string.h>

void damage_init(DamageTracker *dt, int surface_w, int surface_h) {
    if (!dt) return;
    memset(dt, 0, sizeof(DamageTracker));
    dt->surface_w = surface_w;
    dt->surface_h = surface_h;
}

void damage_reset(DamageTracker *dt) {
    if (!dt) return;
    dt->count = 0;
    dt->full  = 0;
}

static void clamp_rect(const DamageTracker *dt, NvRect *r) {
    if (r->x < 0) { r->w += r->x; r->x = 0; }
    if (r->y < 0) { r->h += r->y; r->y = 0; }
    if (r->x + r->w > dt->surface_w) r->w = dt->surface_w - r->x;
    if (r->y + r->h > dt->surface_h) r->h = dt->surface_h - r->y;
    if (r->w < 0) r->w = 0;
    if (r->h < 0) r->h = 0;
}

static int rects_overlap(const NvRect *a, const NvRect *b) {
    return a->x < b->x + b->w && a->x + a->w > b->x &&
           a->y < b->y + b->h && a->y + a->h > b->y;
}

static NvRect rect_union(const NvRect *a, const NvRect *b) {
    int x1 = a->x < b->x ? a->x : b->x;
    int y1 = a->y < b->y ? a->y : b->y;
    int x2 = (a->x + a->w) > (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y2 = (a->y + a->h) > (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    NvRect r = { x1, y1, x2 - x1, y2 - y1 };
    return r;
}

static int rect_area(const NvRect *r) {
    return r->w * r->h;
}

void damage_add(DamageTracker *dt, int x, int y, int w, int h) {
    NvRect r = { x, y, w, h };
    damage_add_rect(dt, &r);
}

void damage_add_rect(DamageTracker *dt, const NvRect *r) {
    if (!dt || !r) return;
    if (dt->full) return;

    NvRect clamped = *r;
    clamp_rect(dt, &clamped);
    if (clamped.w <= 0 || clamped.h <= 0) return;

    /* try to merge with an existing rect if the union is not too much larger */
    for (int i = 0; i < dt->count; i++) {
        NvRect *ex = &dt->rects[i];
        if (rects_overlap(ex, &clamped) ||
            (abs(ex->x - clamped.x) < 8 && abs(ex->y - clamped.y) < 8)) {
            NvRect u = rect_union(ex, &clamped);
            if (rect_area(&u) <= rect_area(ex) + rect_area(&clamped) * 2) {
                *ex = u;
                return;
            }
        }
    }

    if (dt->count >= DAMAGE_MAX_RECTS) {
        dt->full = 1;
        dt->count = 0;
        return;
    }

    dt->rects[dt->count++] = clamped;
}

void damage_add_full(DamageTracker *dt) {
    if (!dt) return;
    dt->full  = 1;
    dt->count = 0;
}

int damage_is_full(const DamageTracker *dt) {
    return dt ? dt->full : 0;
}

int damage_is_empty(const DamageTracker *dt) {
    return dt ? (!dt->full && dt->count == 0) : 1;
}

int damage_count(const DamageTracker *dt) {
    return dt ? dt->count : 0;
}

const NvRect *damage_get(const DamageTracker *dt, int index) {
    if (!dt || index < 0 || index >= dt->count) return NULL;
    return &dt->rects[index];
}

void damage_merge(DamageTracker *dst, const DamageTracker *src) {
    if (!dst || !src) return;
    if (src->full) { damage_add_full(dst); return; }
    for (int i = 0; i < src->count; i++)
        damage_add_rect(dst, &src->rects[i]);
}

void damage_clip_to(DamageTracker *dt, int w, int h) {
    if (!dt) return;
    dt->surface_w = w;
    dt->surface_h = h;
    for (int i = 0; i < dt->count; i++) {
        clamp_rect(dt, &dt->rects[i]);
        if (dt->rects[i].w <= 0 || dt->rects[i].h <= 0) {
            memmove(&dt->rects[i], &dt->rects[i+1],
                    sizeof(NvRect) * (dt->count - i - 1));
            dt->count--;
            i--;
        }
    }
}

NvRect damage_bounding_box(const DamageTracker *dt) {
    NvRect box = { 0, 0, 0, 0 };
    if (!dt || dt->count == 0) return box;
    if (dt->full) {
        box.x = 0; box.y = 0;
        box.w = dt->surface_w;
        box.h = dt->surface_h;
        return box;
    }
    box = dt->rects[0];
    for (int i = 1; i < dt->count; i++)
        box = rect_union(&box, &dt->rects[i]);
    return box;
}