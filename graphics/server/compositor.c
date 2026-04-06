#include "compositor.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

struct Compositor {
    NvSurface   *fb;
    NvSurface   *back;

    CompSurface  surfaces[COMPOSITOR_MAX_SURFACES];
    int          surface_count;
    int          next_id;

    DamageRect   damage[COMPOSITOR_MAX_DAMAGE];
    int          damage_count;
    int          full_redraw;
};

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

Compositor *compositor_new(uint32_t *framebuffer, int fb_w, int fb_h,
                            int fb_pitch) {
    Compositor *c = xmalloc(sizeof(Compositor));
    if (!c) return NULL;

    c->fb   = surface_from_pixels(framebuffer, fb_w, fb_h, fb_pitch);
    c->back = surface_new(fb_w, fb_h);
    if (!c->fb || !c->back) {
        surface_free(c->fb);
        surface_free(c->back);
        free(c);
        return NULL;
    }

    c->next_id    = 1;
    c->full_redraw = 1;
    return c;
}

void compositor_free(Compositor *c) {
    if (!c) return;
    for (int i = 0; i < c->surface_count; i++)
        surface_free(c->surfaces[i].buffer);
    surface_free(c->fb);
    surface_free(c->back);
    free(c);
}

/* -----------------------------------------------------------------------
 * Surface management
 * --------------------------------------------------------------------- */

static int find_surface(Compositor *c, int id) {
    for (int i = 0; i < c->surface_count; i++)
        if (c->surfaces[i].id == id) return i;
    return -1;
}

static void sort_by_z(Compositor *c) {
    for (int i = 1; i < c->surface_count; i++) {
        CompSurface tmp = c->surfaces[i];
        int j = i - 1;
        while (j >= 0 && c->surfaces[j].z_order > tmp.z_order) {
            c->surfaces[j+1] = c->surfaces[j];
            j--;
        }
        c->surfaces[j+1] = tmp;
    }
}

int compositor_create_surface(Compositor *c, int x, int y,
                               int w, int h, SurfaceLayer layer) {
    if (!c || c->surface_count >= COMPOSITOR_MAX_SURFACES) return -1;
    if (w <= 0 || h <= 0) return -1;

    CompSurface *s = &c->surfaces[c->surface_count];
    memset(s, 0, sizeof(CompSurface));

    s->buffer = surface_new(w, h);
    if (!s->buffer) return -1;

    s->id      = c->next_id++;
    s->x       = x;
    s->y       = y;
    s->w       = w;
    s->h       = h;
    s->alpha   = 255;
    s->visible = 1;
    s->dirty   = 1;
    s->layer   = layer;

    s->z_order = (layer == SURFACE_WALLPAPER)    ? 0   :
                 (layer == SURFACE_NORMAL)        ? 100 :
                 (layer == SURFACE_OVERLAY)       ? 200 :
                 (layer == SURFACE_NOTIFICATION)  ? 300 :
                 (layer == SURFACE_CURSOR)        ? 400 : 100;

    c->surface_count++;
    sort_by_z(c);
    c->full_redraw = 1;
    return s->id;
}

void compositor_destroy_surface(Compositor *c, int id) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;

    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                      c->surfaces[idx].w, c->surfaces[idx].h);

    surface_free(c->surfaces[idx].buffer);
    memmove(&c->surfaces[idx], &c->surfaces[idx+1],
            sizeof(CompSurface) * (c->surface_count - idx - 1));
    c->surface_count--;
}

CompSurface *compositor_get_surface(Compositor *c, int id) {
    if (!c) return NULL;
    int idx = find_surface(c, id);
    return idx >= 0 ? &c->surfaces[idx] : NULL;
}

void compositor_move_surface(Compositor *c, int id, int x, int y) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                      c->surfaces[idx].w, c->surfaces[idx].h);
    c->surfaces[idx].x = x;
    c->surfaces[idx].y = y;
    c->surfaces[idx].dirty = 1;
    compositor_damage(c, x, y, c->surfaces[idx].w, c->surfaces[idx].h);
}

void compositor_resize_surface(Compositor *c, int id, int w, int h) {
    if (!c || w <= 0 || h <= 0) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;

    surface_free(c->surfaces[idx].buffer);
    c->surfaces[idx].buffer = surface_new(w, h);
    if (!c->surfaces[idx].buffer) return;

    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                      c->surfaces[idx].w, c->surfaces[idx].h);
    c->surfaces[idx].w     = w;
    c->surfaces[idx].h     = h;
    c->surfaces[idx].dirty = 1;
    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y, w, h);
}

void compositor_set_alpha(Compositor *c, int id, int alpha) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    c->surfaces[idx].alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    c->surfaces[idx].dirty = 1;
    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                      c->surfaces[idx].w, c->surfaces[idx].h);
}

void compositor_show_surface(Compositor *c, int id) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    if (!c->surfaces[idx].visible) {
        c->surfaces[idx].visible = 1;
        c->surfaces[idx].dirty   = 1;
        compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                          c->surfaces[idx].w, c->surfaces[idx].h);
    }
}

void compositor_hide_surface(Compositor *c, int id) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    if (c->surfaces[idx].visible) {
        c->surfaces[idx].visible = 0;
        compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                          c->surfaces[idx].w, c->surfaces[idx].h);
    }
}

void compositor_raise_surface(Compositor *c, int id) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    int max_z = 0;
    for (int i = 0; i < c->surface_count; i++) {
        if (c->surfaces[i].layer == c->surfaces[idx].layer &&
            c->surfaces[i].z_order > max_z)
            max_z = c->surfaces[i].z_order;
    }
    c->surfaces[idx].z_order = max_z + 1;
    sort_by_z(c);
    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                      c->surfaces[idx].w, c->surfaces[idx].h);
}

void compositor_lower_surface(Compositor *c, int id) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    if (c->surfaces[idx].z_order > 0) {
        c->surfaces[idx].z_order--;
        sort_by_z(c);
        compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                          c->surfaces[idx].w, c->surfaces[idx].h);
    }
}

/* -----------------------------------------------------------------------
 * Damage tracking
 * --------------------------------------------------------------------- */

void compositor_damage(Compositor *c, int x, int y, int w, int h) {
    if (!c || w <= 0 || h <= 0) return;

    if (c->damage_count >= COMPOSITOR_MAX_DAMAGE) {
        c->full_redraw = 1;
        return;
    }

    for (int i = 0; i < c->damage_count; i++) {
        DamageRect *d = &c->damage[i];
        int dx2 = d->x + d->w, dy2 = d->y + d->h;
        int nx2 = x + w,       ny2 = y + h;

        int ox = x < d->x ? x : d->x;
        int oy = y < d->y ? y : d->y;
        int ow = (nx2 > dx2 ? nx2 : dx2) - ox;
        int oh = (ny2 > dy2 ? ny2 : dy2) - oy;

        int merged_area = ow * oh;
        int sum_area    = w * h + d->w * d->h;

        if (merged_area <= sum_area * 3 / 2) {
            d->x = ox; d->y = oy; d->w = ow; d->h = oh;
            return;
        }
    }

    c->damage[c->damage_count].x = x;
    c->damage[c->damage_count].y = y;
    c->damage[c->damage_count].w = w;
    c->damage[c->damage_count].h = h;
    c->damage_count++;
}

void compositor_damage_surface(Compositor *c, int id) {
    if (!c) return;
    int idx = find_surface(c, id);
    if (idx < 0) return;
    c->surfaces[idx].dirty = 1;
    compositor_damage(c, c->surfaces[idx].x, c->surfaces[idx].y,
                      c->surfaces[idx].w, c->surfaces[idx].h);
}

/* -----------------------------------------------------------------------
 * Compositing
 * --------------------------------------------------------------------- */

static void composite_region(Compositor *c, int rx, int ry, int rw, int rh) {
    if (rw <= 0 || rh <= 0) return;

    int fb_w = c->fb->width, fb_h = c->fb->height;
    if (rx < 0) { rw += rx; rx = 0; }
    if (ry < 0) { rh += ry; ry = 0; }
    if (rx + rw > fb_w) rw = fb_w - rx;
    if (ry + rh > fb_h) rh = fb_h - ry;
    if (rw <= 0 || rh <= 0) return;

    NvRect clear = { rx, ry, rw, rh };
    draw_fill_rect(c->back, &clear, 0xFF1A1A2EFF);

    for (int i = 0; i < c->surface_count; i++) {
        CompSurface *s = &c->surfaces[i];
        if (!s->visible || !s->buffer) continue;

        int sx1 = s->x, sy1 = s->y;
        int sx2 = s->x + s->w, sy2 = s->y + s->h;

        int ox = rx > sx1 ? rx : sx1;
        int oy = ry > sy1 ? ry : sy1;
        int ox2 = (rx + rw) < sx2 ? (rx + rw) : sx2;
        int oy2 = (ry + rh) < sy2 ? (ry + rh) : sy2;
        int ow = ox2 - ox, oh = oy2 - oy;
        if (ow <= 0 || oh <= 0) continue;

        int src_x = ox - s->x;
        int src_y = oy - s->y;

        if (s->alpha == 255) {
            surface_blit(c->back, s->buffer, ox, oy, src_x, src_y, ow, oh);
        } else {
            for (int py = 0; py < oh; py++) {
                for (int px = 0; px < ow; px++) {
                    uint32_t sp = s->buffer->pixels[
                        (src_y + py) * s->buffer->pitch + (src_x + px)];
                    uint32_t sa = COLOR_A(sp);
                    sa = (sa * s->alpha) / 255;
                    sp = color_with_alpha(sp, (uint8_t)sa);
                    uint32_t *dp = &c->back->pixels[
                        (oy + py) * c->back->pitch + (ox + px)];
                    *dp = color_blend(*dp, sp);
                }
            }
        }
        s->dirty = 0;
    }

    surface_blit(c->fb, c->back, rx, ry, rx, ry, rw, rh);
}

void compositor_composite(Compositor *c) {
    if (!c) return;

    if (c->full_redraw) {
        composite_region(c, 0, 0, c->fb->width, c->fb->height);
        c->full_redraw  = 0;
        c->damage_count = 0;
        return;
    }

    for (int i = 0; i < c->damage_count; i++) {
        DamageRect *d = &c->damage[i];
        composite_region(c, d->x, d->y, d->w, d->h);
    }
    c->damage_count = 0;
}

NvSurface *compositor_surface_buffer(Compositor *c, int id) {
    if (!c) return NULL;
    int idx = find_surface(c, id);
    return idx >= 0 ? c->surfaces[idx].buffer : NULL;
}

int compositor_fb_width(const Compositor *c)  { return c ? c->fb->width  : 0; }
int compositor_fb_height(const Compositor *c) { return c ? c->fb->height : 0; }