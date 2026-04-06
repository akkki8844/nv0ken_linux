#include "draw.h"
#include "color.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Surface management
 * --------------------------------------------------------------------- */

NvSurface *surface_new(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    NvSurface *s = malloc(sizeof(NvSurface));
    if (!s) return NULL;
    s->pixels = calloc(width * height, sizeof(uint32_t));
    if (!s->pixels) { free(s); return NULL; }
    s->width       = width;
    s->height      = height;
    s->pitch       = width;
    s->owns_pixels = 1;
    return s;
}

NvSurface *surface_from_pixels(uint32_t *pixels, int w, int h, int pitch) {
    NvSurface *s = malloc(sizeof(NvSurface));
    if (!s) return NULL;
    s->pixels      = pixels;
    s->width       = w;
    s->height      = h;
    s->pitch       = pitch > 0 ? pitch : w;
    s->owns_pixels = 0;
    return s;
}

void surface_free(NvSurface *s) {
    if (!s) return;
    if (s->owns_pixels) free(s->pixels);
    free(s);
}

void surface_clear(NvSurface *s, uint32_t color) {
    if (!s || !s->pixels) return;
    int total = s->pitch * s->height;
    for (int i = 0; i < total; i++) s->pixels[i] = color;
}

NvSurface *surface_clone(const NvSurface *s) {
    if (!s) return NULL;
    NvSurface *c = surface_new(s->width, s->height);
    if (!c) return NULL;
    for (int y = 0; y < s->height; y++)
        memcpy(c->pixels + y * c->pitch,
               s->pixels + y * s->pitch,
               s->width * sizeof(uint32_t));
    return c;
}

void surface_blit(NvSurface *dst, const NvSurface *src,
                  int dx, int dy, int sx, int sy, int w, int h) {
    if (!dst || !src) return;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (dx + w > dst->width)  w = dst->width  - dx;
    if (dy + h > dst->height) h = dst->height - dy;
    if (sx + w > src->width)  w = src->width  - sx;
    if (sy + h > src->height) h = src->height - sy;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++)
        memcpy(dst->pixels + (dy + row) * dst->pitch + dx,
               src->pixels + (sy + row) * src->pitch + sx,
               w * sizeof(uint32_t));
}

void surface_blit_alpha(NvSurface *dst, const NvSurface *src,
                         int dx, int dy, int sx, int sy, int w, int h) {
    if (!dst || !src) return;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dst->width)  w = dst->width  - dx;
    if (dy + h > dst->height) h = dst->height - dy;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint32_t sp = src->pixels[(sy + row) * src->pitch + (sx + col)];
            uint32_t *dp = &dst->pixels[(dy + row) * dst->pitch + (dx + col)];
            *dp = color_blend(*dp, sp);
        }
    }
}

/* -----------------------------------------------------------------------
 * Pixel access
 * --------------------------------------------------------------------- */

void draw_pixel(NvSurface *s, int x, int y, uint32_t color) {
    if (!s || x < 0 || y < 0 || x >= s->width || y >= s->height) return;
    uint32_t *p = &s->pixels[y * s->pitch + x];
    if (COLOR_A(color) == 255) { *p = color; return; }
    if (COLOR_A(color) == 0)   return;
    *p = color_blend(*p, color);
}

uint32_t draw_get_pixel(const NvSurface *s, int x, int y) {
    if (!s || x < 0 || y < 0 || x >= s->width || y >= s->height) return 0;
    return s->pixels[y * s->pitch + x];
}

/* -----------------------------------------------------------------------
 * Rectangles
 * --------------------------------------------------------------------- */

void draw_fill_rect(NvSurface *s, const NvRect *r, uint32_t color) {
    if (!s || !r || r->w <= 0 || r->h <= 0) return;
    int x0 = r->x, y0 = r->y;
    int x1 = r->x + r->w, y1 = r->y + r->h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s->width)  x1 = s->width;
    if (y1 > s->height) y1 = s->height;
    if (x0 >= x1 || y0 >= y1) return;

    if (COLOR_A(color) == 255) {
        for (int y = y0; y < y1; y++) {
            uint32_t *row = s->pixels + y * s->pitch + x0;
            int w = x1 - x0;
            for (int i = 0; i < w; i++) row[i] = color;
        }
    } else {
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                uint32_t *p = &s->pixels[y * s->pitch + x];
                *p = color_blend(*p, color);
            }
    }
}

void draw_fill_rect_alpha(NvSurface *s, const NvRect *r, uint32_t color) {
    draw_fill_rect(s, r, color);
}

void draw_rect(NvSurface *s, const NvRect *r, uint32_t color) {
    if (!s || !r) return;
    draw_hline(s, r->x, r->y,           r->w, color);
    draw_hline(s, r->x, r->y + r->h - 1, r->w, color);
    draw_vline(s, r->x,           r->y, r->h, color);
    draw_vline(s, r->x + r->w - 1, r->y, r->h, color);
}

void draw_rect_thick(NvSurface *s, const NvRect *r, uint32_t color, int t) {
    if (!s || !r || t <= 0) return;
    for (int i = 0; i < t; i++) {
        NvRect inner = { r->x + i, r->y + i, r->w - i*2, r->h - i*2 };
        draw_rect(s, &inner, color);
    }
}

void draw_fill_rounded_rect(NvSurface *s, const NvRect *r,
                              uint32_t color, int radius) {
    if (!s || !r) return;
    if (radius <= 0) { draw_fill_rect(s, r, color); return; }
    int max_r = (r->w < r->h ? r->w : r->h) / 2;
    if (radius > max_r) radius = max_r;

    NvRect mid = { r->x, r->y + radius, r->w, r->h - radius * 2 };
    draw_fill_rect(s, &mid, color);

    NvRect top = { r->x + radius, r->y, r->w - radius * 2, radius };
    draw_fill_rect(s, &top, color);

    NvRect bot = { r->x + radius, r->y + r->h - radius,
                   r->w - radius * 2, radius };
    draw_fill_rect(s, &bot, color);

    int cx_tl = r->x + radius, cy_tl = r->y + radius;
    int cx_tr = r->x + r->w - radius - 1, cy_tr = r->y + radius;
    int cx_bl = r->x + radius, cy_bl = r->y + r->h - radius - 1;
    int cx_br = r->x + r->w - radius - 1, cy_br = r->y + r->h - radius - 1;

    for (int dy = 0; dy <= radius; dy++) {
        for (int dx = 0; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                draw_pixel(s, cx_tl - dx, cy_tl - dy, color);
                draw_pixel(s, cx_tr + dx, cy_tr - dy, color);
                draw_pixel(s, cx_bl - dx, cy_bl + dy, color);
                draw_pixel(s, cx_br + dx, cy_br + dy, color);
            }
        }
    }
}

void draw_rounded_rect(NvSurface *s, const NvRect *r, uint32_t color, int radius) {
    if (!s || !r) return;
    if (radius <= 0) { draw_rect(s, r, color); return; }

    int x0 = r->x, y0 = r->y;
    int x1 = r->x + r->w - 1, y1 = r->y + r->h - 1;

    draw_hline(s, x0 + radius, y0, r->w - 2 * radius, color);
    draw_hline(s, x0 + radius, y1, r->w - 2 * radius, color);
    draw_vline(s, x0, y0 + radius, r->h - 2 * radius, color);
    draw_vline(s, x1, y0 + radius, r->h - 2 * radius, color);

    int cx_tl = x0 + radius, cy_tl = y0 + radius;
    int cx_tr = x1 - radius, cy_tr = y0 + radius;
    int cx_bl = x0 + radius, cy_bl = y1 - radius;
    int cx_br = x1 - radius, cy_br = y1 - radius;

    int x = 0, y = radius, d = 3 - 2 * radius;
    while (x <= y) {
        draw_pixel(s, cx_tl - x, cy_tl - y, color);
        draw_pixel(s, cx_tl - y, cy_tl - x, color);
        draw_pixel(s, cx_tr + x, cy_tr - y, color);
        draw_pixel(s, cx_tr + y, cy_tr - x, color);
        draw_pixel(s, cx_bl - x, cy_bl + y, color);
        draw_pixel(s, cx_bl - y, cy_bl + x, color);
        draw_pixel(s, cx_br + x, cy_br + y, color);
        draw_pixel(s, cx_br + y, cy_br + x, color);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

/* -----------------------------------------------------------------------
 * Lines
 * --------------------------------------------------------------------- */

void draw_hline(NvSurface *s, int x, int y, int w, uint32_t color) {
    if (!s || w <= 0 || y < 0 || y >= s->height) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > s->width) w = s->width - x;
    if (w <= 0) return;
    uint32_t *row = s->pixels + y * s->pitch + x;
    if (COLOR_A(color) == 255) {
        for (int i = 0; i < w; i++) row[i] = color;
    } else {
        for (int i = 0; i < w; i++) row[i] = color_blend(row[i], color);
    }
}

void draw_vline(NvSurface *s, int x, int y, int h, uint32_t color) {
    if (!s || h <= 0 || x < 0 || x >= s->width) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > s->height) h = s->height - y;
    if (h <= 0) return;
    for (int i = 0; i < h; i++) {
        uint32_t *p = &s->pixels[(y + i) * s->pitch + x];
        *p = COLOR_A(color) == 255 ? color : color_blend(*p, color);
    }
}

void draw_line(NvSurface *s, int x0, int y0, int x1, int y1, uint32_t color) {
    if (!s) return;
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        draw_pixel(s, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

void draw_line_thick(NvSurface *s, int x0, int y0, int x1, int y1,
                     uint32_t color, int thickness) {
    if (!s || thickness <= 1) { draw_line(s, x0, y0, x1, y1, color); return; }
    int half = thickness / 2;
    int dx = x1 - x0, dy = y1 - y0;
    float len = (float)(dx*dx + dy*dy);
    if (len == 0.0f) {
        NvRect r = { x0 - half, y0 - half, thickness, thickness };
        draw_fill_rect(s, &r, color);
        return;
    }
    float inv = 1.0f / len;
    float nx = -dy * inv, ny = dx * inv;
    for (int i = -half; i <= half; i++) {
        int ox = (int)(nx * i), oy = (int)(ny * i);
        draw_line(s, x0 + ox, y0 + oy, x1 + ox, y1 + oy, color);
    }
}

/* -----------------------------------------------------------------------
 * Circles and ellipses
 * --------------------------------------------------------------------- */

void draw_circle(NvSurface *s, int cx, int cy, int r, uint32_t color) {
    if (!s || r <= 0) return;
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        draw_pixel(s, cx+x, cy+y, color); draw_pixel(s, cx-x, cy+y, color);
        draw_pixel(s, cx+x, cy-y, color); draw_pixel(s, cx-x, cy-y, color);
        draw_pixel(s, cx+y, cy+x, color); draw_pixel(s, cx-y, cy+x, color);
        draw_pixel(s, cx+y, cy-x, color); draw_pixel(s, cx-y, cy-x, color);
        if (d < 0) d += 4*x + 6;
        else { d += 4*(x-y) + 10; y--; }
        x++;
    }
}

void draw_fill_circle(NvSurface *s, int cx, int cy, int r, uint32_t color) {
    if (!s || r <= 0) return;
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        draw_hline(s, cx - x, cy + y, 2*x + 1, color);
        draw_hline(s, cx - x, cy - y, 2*x + 1, color);
        draw_hline(s, cx - y, cy + x, 2*y + 1, color);
        draw_hline(s, cx - y, cy - x, 2*y + 1, color);
        if (d < 0) d += 4*x + 6;
        else { d += 4*(x-y) + 10; y--; }
        x++;
    }
}

void draw_ellipse(NvSurface *s, int cx, int cy, int rx, int ry, uint32_t color) {
    if (!s) return;
    int x = 0, y = ry;
    long dx = 2 * ry * ry * x;
    long dy = 2 * rx * rx * y;
    long d  = ry * ry - rx * rx * ry + rx * rx / 4;
    while (dx < dy) {
        draw_pixel(s, cx+x, cy+y, color); draw_pixel(s, cx-x, cy+y, color);
        draw_pixel(s, cx+x, cy-y, color); draw_pixel(s, cx-x, cy-y, color);
        if (d < 0) { d += ry*ry*(2*x+3); dx += 2*ry*ry; x++; }
        else { d += ry*ry*(2*x+3) + rx*rx*(-2*y+2); dx += 2*ry*ry; dy -= 2*rx*rx; x++; y--; }
    }
    d = ry*ry*(x+(long)1/2)*(x+(long)1/2) + rx*rx*(y-1)*(y-1) - rx*rx*ry*ry;
    while (y >= 0) {
        draw_pixel(s, cx+x, cy+y, color); draw_pixel(s, cx-x, cy+y, color);
        draw_pixel(s, cx+x, cy-y, color); draw_pixel(s, cx-x, cy-y, color);
        if (d > 0) { d += rx*rx*(-2*y+3); y--; }
        else { d += ry*ry*(2*x+2) + rx*rx*(-2*y+3); x++; y--; }
    }
}

void draw_fill_ellipse(NvSurface *s, int cx, int cy, int rx, int ry, uint32_t color) {
    if (!s) return;
    for (int py = -ry; py <= ry; py++) {
        float ratio = (float)py / ry;
        int half_w  = (int)(rx * (float)(__builtin_sqrt(1.0 - ratio*ratio)));
        draw_hline(s, cx - half_w, cy + py, 2*half_w + 1, color);
    }
}

/* -----------------------------------------------------------------------
 * Triangles
 * --------------------------------------------------------------------- */

void draw_triangle(NvSurface *s, int x0, int y0, int x1, int y1,
                   int x2, int y2, uint32_t color) {
    draw_line(s, x0, y0, x1, y1, color);
    draw_line(s, x1, y1, x2, y2, color);
    draw_line(s, x2, y2, x0, y0, color);
}

static void sort3(int *ax, int *ay, int *bx, int *by, int *cx, int *cy) {
    if (*ay > *by) { int t; t=*ax;*ax=*bx;*bx=t; t=*ay;*ay=*by;*by=t; }
    if (*ay > *cy) { int t; t=*ax;*ax=*cx;*cx=t; t=*ay;*ay=*cy;*cy=t; }
    if (*by > *cy) { int t; t=*bx;*bx=*cx;*cx=t; t=*by;*by=*cy;*cy=t; }
}

void draw_fill_triangle(NvSurface *s, int x0, int y0, int x1, int y1,
                        int x2, int y2, uint32_t color) {
    if (!s) return;
    sort3(&x0, &y0, &x1, &y1, &x2, &y2);

    for (int y = y0; y <= y2; y++) {
        float t1 = (y2 == y0) ? 0.0f : (float)(y - y0) / (y2 - y0);
        int lx = x0 + (int)((x2 - x0) * t1);
        int rx = lx;

        if (y < y1) {
            float t2 = (y1 == y0) ? 0.0f : (float)(y - y0) / (y1 - y0);
            rx = x0 + (int)((x1 - x0) * t2);
        } else {
            float t2 = (y2 == y1) ? 0.0f : (float)(y - y1) / (y2 - y1);
            rx = x1 + (int)((x2 - x1) * t2);
        }

        if (lx > rx) { int t = lx; lx = rx; rx = t; }
        draw_hline(s, lx, y, rx - lx + 1, color);
    }
}

/* -----------------------------------------------------------------------
 * Gradients
 * --------------------------------------------------------------------- */

void draw_gradient_h(NvSurface *s, const NvRect *r,
                     uint32_t left, uint32_t right) {
    if (!s || !r) return;
    for (int y = r->y; y < r->y + r->h; y++) {
        for (int x = 0; x < r->w; x++) {
            uint8_t t = (uint8_t)((x * 255) / (r->w - 1));
            draw_pixel(s, r->x + x, y, color_lerp(left, right, t));
        }
    }
}

void draw_gradient_v(NvSurface *s, const NvRect *r,
                     uint32_t top, uint32_t bottom) {
    if (!s || !r) return;
    for (int y = 0; y < r->h; y++) {
        uint8_t t = (uint8_t)((y * 255) / (r->h - 1));
        draw_hline(s, r->x, r->y + y, r->w, color_lerp(top, bottom, t));
    }
}

/* -----------------------------------------------------------------------
 * Region copy
 * --------------------------------------------------------------------- */

void draw_copy_region(NvSurface *s, int dx, int dy,
                      int sx, int sy, int w, int h) {
    if (!s) return;
    if (dy > sy) {
        for (int row = h - 1; row >= 0; row--)
            memmove(s->pixels + (dy + row) * s->pitch + dx,
                    s->pixels + (sy + row) * s->pitch + sx,
                    w * sizeof(uint32_t));
    } else {
        for (int row = 0; row < h; row++)
            memmove(s->pixels + (dy + row) * s->pitch + dx,
                    s->pixels + (sy + row) * s->pitch + sx,
                    w * sizeof(uint32_t));
    }
}

/* -----------------------------------------------------------------------
 * Scaled image blit
 * --------------------------------------------------------------------- */

void draw_image_scaled(NvSurface *dst, const uint32_t *src_pixels,
                       int src_w, int src_h,
                       int dst_x, int dst_y, int dst_w, int dst_h) {
    if (!dst || !src_pixels || dst_w <= 0 || dst_h <= 0) return;
    for (int py = 0; py < dst_h; py++) {
        int sy = (py * src_h) / dst_h;
        if (sy >= src_h) sy = src_h - 1;
        for (int px = 0; px < dst_w; px++) {
            int sx = (px * src_w) / dst_w;
            if (sx >= src_w) sx = src_w - 1;
            uint32_t c = src_pixels[sy * src_w + sx];
            draw_pixel(dst, dst_x + px, dst_y + py, c);
        }
    }
}