#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include <stddef.h>
#include "color.h"

typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
    int       pitch;
    int       owns_pixels;
} NvSurface;

typedef struct {
    int x, y, w, h;
} NvRect;

typedef struct {
    int x, y;
} NvPoint;

NvSurface *surface_new(int width, int height);
NvSurface *surface_from_pixels(uint32_t *pixels, int w, int h, int pitch);
void       surface_free(NvSurface *s);
void       surface_clear(NvSurface *s, uint32_t color);
NvSurface *surface_clone(const NvSurface *s);
void       surface_blit(NvSurface *dst, const NvSurface *src,
                        int dx, int dy, int sx, int sy, int w, int h);
void       surface_blit_alpha(NvSurface *dst, const NvSurface *src,
                               int dx, int dy, int sx, int sy, int w, int h);

void draw_pixel(NvSurface *s, int x, int y, uint32_t color);
uint32_t draw_get_pixel(const NvSurface *s, int x, int y);

void draw_fill_rect(NvSurface *s, const NvRect *r, uint32_t color);
void draw_fill_rect_alpha(NvSurface *s, const NvRect *r, uint32_t color);
void draw_rect(NvSurface *s, const NvRect *r, uint32_t color);
void draw_rect_thick(NvSurface *s, const NvRect *r, uint32_t color, int thickness);
void draw_rounded_rect(NvSurface *s, const NvRect *r, uint32_t color, int radius);
void draw_fill_rounded_rect(NvSurface *s, const NvRect *r, uint32_t color, int radius);

void draw_line(NvSurface *s, int x0, int y0, int x1, int y1, uint32_t color);
void draw_line_thick(NvSurface *s, int x0, int y0, int x1, int y1,
                     uint32_t color, int thickness);
void draw_hline(NvSurface *s, int x, int y, int w, uint32_t color);
void draw_vline(NvSurface *s, int x, int y, int h, uint32_t color);

void draw_circle(NvSurface *s, int cx, int cy, int r, uint32_t color);
void draw_fill_circle(NvSurface *s, int cx, int cy, int r, uint32_t color);
void draw_ellipse(NvSurface *s, int cx, int cy, int rx, int ry, uint32_t color);
void draw_fill_ellipse(NvSurface *s, int cx, int cy, int rx, int ry, uint32_t color);

void draw_triangle(NvSurface *s, int x0, int y0, int x1, int y1,
                   int x2, int y2, uint32_t color);
void draw_fill_triangle(NvSurface *s, int x0, int y0, int x1, int y1,
                        int x2, int y2, uint32_t color);

void draw_gradient_h(NvSurface *s, const NvRect *r,
                     uint32_t left, uint32_t right);
void draw_gradient_v(NvSurface *s, const NvRect *r,
                     uint32_t top, uint32_t bottom);

void draw_copy_region(NvSurface *s, int dx, int dy,
                      int sx, int sy, int w, int h);

void draw_image_scaled(NvSurface *dst, const uint32_t *src_pixels,
                       int src_w, int src_h,
                       int dst_x, int dst_y, int dst_w, int dst_h);

static inline int rect_contains(const NvRect *r, int x, int y) {
    return x >= r->x && x < r->x + r->w &&
           y >= r->y && y < r->y + r->h;
}

static inline NvRect rect_intersect(const NvRect *a, const NvRect *b) {
    int x1 = a->x > b->x ? a->x : b->x;
    int y1 = a->y > b->y ? a->y : b->y;
    int x2 = (a->x+a->w) < (b->x+b->w) ? (a->x+a->w) : (b->x+b->w);
    int y2 = (a->y+a->h) < (b->y+b->h) ? (a->y+a->h) : (b->y+b->h);
    NvRect r = { x1, y1, x2-x1, y2-y1 };
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

static inline int rect_empty(const NvRect *r) {
    return r->w <= 0 || r->h <= 0;
}

#endif