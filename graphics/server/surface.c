#include "surface.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

static int g_next_surface_id = 1;

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

ServerSurface *server_surface_new(int w, int h, SurfaceFormat fmt, int flags) {
    if (w <= 0 || h <= 0) return NULL;

    ServerSurface *s = xmalloc(sizeof(ServerSurface));
    if (!s) return NULL;

    s->pixels = calloc(w * h, sizeof(uint32_t));
    if (!s->pixels) { free(s); return NULL; }

    s->id          = g_next_surface_id++;
    s->width       = w;
    s->height      = h;
    s->pitch       = w;
    s->format      = fmt;
    s->flags       = flags;
    s->dirty       = 0;
    s->owns_pixels = 1;
    s->shm_id      = -1;
    return s;
}

ServerSurface *server_surface_from_shm(int shm_id, int w, int h,
                                        SurfaceFormat fmt) {
    if (shm_id < 0 || w <= 0 || h <= 0) return NULL;

    void *ptr = (void *)(long)sys_shmat(shm_id, NULL, 0);
    if (!ptr || ptr == (void *)-1) return NULL;

    ServerSurface *s = xmalloc(sizeof(ServerSurface));
    if (!s) { sys_shmdt(ptr); return NULL; }

    s->id          = g_next_surface_id++;
    s->pixels      = (uint32_t *)ptr;
    s->width       = w;
    s->height      = h;
    s->pitch       = w;
    s->format      = fmt;
    s->flags       = SURFACE_FLAG_SHARED_MEM;
    s->dirty       = 0;
    s->owns_pixels = 0;
    s->shm_id      = shm_id;
    return s;
}

void server_surface_free(ServerSurface *s) {
    if (!s) return;
    if (s->owns_pixels) {
        free(s->pixels);
    } else if (s->flags & SURFACE_FLAG_SHARED_MEM) {
        sys_shmdt(s->pixels);
    }
    free(s);
}

void server_surface_clear(ServerSurface *s, uint32_t color) {
    if (!s || !s->pixels) return;
    int total = s->pitch * s->height;
    for (int i = 0; i < total; i++) s->pixels[i] = color;
    s->dirty = 1;
}

void server_surface_mark_dirty(ServerSurface *s) {
    if (s) s->dirty = 1;
}

void server_surface_blit_to(const ServerSurface *s, NvSurface *dst,
                              int dx, int dy, int sx, int sy, int w, int h) {
    if (!s || !dst || !s->pixels) return;
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (w <= 0 || h <= 0) return;
    if (sx + w > s->width)   w = s->width  - sx;
    if (sy + h > s->height)  h = s->height - sy;
    if (dx + w > dst->width) w = dst->width  - dx;
    if (dy + h > dst->height)h = dst->height - dy;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        memcpy(dst->pixels + (dy + row) * dst->pitch + dx,
               s->pixels   + (sy + row) * s->pitch   + sx,
               w * sizeof(uint32_t));
    }
}

void server_surface_blit_alpha_to(const ServerSurface *s, NvSurface *dst,
                                   int dx, int dy, int sx, int sy,
                                   int w, int h, uint8_t global_alpha) {
    if (!s || !dst || !s->pixels) return;
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (w <= 0 || h <= 0) return;
    if (sx + w > s->width)    w = s->width  - sx;
    if (sy + h > s->height)   h = s->height - sy;
    if (dx + w > dst->width)  w = dst->width  - dx;
    if (dy + h > dst->height) h = dst->height - dy;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        const uint32_t *src_row = s->pixels + (sy + row) * s->pitch + sx;
        uint32_t       *dst_row = dst->pixels + (dy + row) * dst->pitch + dx;
        for (int col = 0; col < w; col++) {
            uint32_t sp = src_row[col];
            if (global_alpha < 255) {
                uint32_t a = (COLOR_A(sp) * global_alpha) / 255;
                sp = color_with_alpha(sp, (uint8_t)a);
            }
            dst_row[col] = color_blend(dst_row[col], sp);
        }
    }
}

NvSurface *server_surface_as_nv(ServerSurface *s) {
    if (!s) return NULL;
    return surface_from_pixels(s->pixels, s->width, s->height, s->pitch);
}