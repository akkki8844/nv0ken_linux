#ifndef DAMAGE_H
#define DAMAGE_H

#include "../draw/draw.h"

#define DAMAGE_MAX_RECTS  64

typedef struct {
    NvRect rects[DAMAGE_MAX_RECTS];
    int    count;
    int    full;
    int    surface_w;
    int    surface_h;
} DamageTracker;

void        damage_init(DamageTracker *dt, int surface_w, int surface_h);
void        damage_reset(DamageTracker *dt);
void        damage_add(DamageTracker *dt, int x, int y, int w, int h);
void        damage_add_rect(DamageTracker *dt, const NvRect *r);
void        damage_add_full(DamageTracker *dt);
int         damage_is_full(const DamageTracker *dt);
int         damage_is_empty(const DamageTracker *dt);
int         damage_count(const DamageTracker *dt);
const NvRect *damage_get(const DamageTracker *dt, int index);
void        damage_merge(DamageTracker *dst, const DamageTracker *src);
void        damage_clip_to(DamageTracker *dt, int w, int h);
NvRect      damage_bounding_box(const DamageTracker *dt);

#endif