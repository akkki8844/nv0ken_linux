#include "canvas.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

/* -----------------------------------------------------------------------
 * View transform helpers
 * --------------------------------------------------------------------- */

/* widget coordinates → canvas pixel coordinates */
int canvas_to_surface_x(const Canvas *cv, int wx) {
    if (!cv || cv->zoom == 0.0f) return 0;
    return (int)((wx - cv->base.x - cv->pan_x) / cv->zoom);
}

int canvas_to_surface_y(const Canvas *cv, int wy) {
    if (!cv || cv->zoom == 0.0f) return 0;
    return (int)((wy - cv->base.y - cv->pan_y) / cv->zoom);
}

/* canvas pixel → widget coordinates */
int canvas_from_surface_x(const Canvas *cv, int sx) {
    if (!cv) return 0;
    return cv->base.x + cv->pan_x + (int)(sx * cv->zoom);
}

int canvas_from_surface_y(const Canvas *cv, int sy) {
    if (!cv) return 0;
    return cv->base.y + cv->pan_y + (int)(sy * cv->zoom);
}

/* -----------------------------------------------------------------------
 * Compute the destination rectangle on the widget surface for the canvas
 * --------------------------------------------------------------------- */

static void compute_dest_rect(const Canvas *cv,
                               int *out_x, int *out_y,
                               int *out_w, int *out_h) {
    int w = cv->base.w;
    int h = cv->base.h;

    switch (cv->scale_mode) {
        case CANVAS_SCALE_NONE:
            *out_x = cv->base.x + cv->pan_x;
            *out_y = cv->base.y + cv->pan_y;
            *out_w = cv->canvas_w;
            *out_h = cv->canvas_h;
            break;

        case CANVAS_SCALE_FIT: {
            float sx = (float)w / cv->canvas_w;
            float sy = (float)h / cv->canvas_h;
            float s  = sx < sy ? sx : sy;
            *out_w   = (int)(cv->canvas_w * s);
            *out_h   = (int)(cv->canvas_h * s);
            *out_x   = cv->base.x + (w - *out_w) / 2 + cv->pan_x;
            *out_y   = cv->base.y + (h - *out_h) / 2 + cv->pan_y;
            break;
        }

        case CANVAS_SCALE_FILL: {
            float sx = (float)w / cv->canvas_w;
            float sy = (float)h / cv->canvas_h;
            float s  = sx > sy ? sx : sy;
            *out_w   = (int)(cv->canvas_w * s);
            *out_h   = (int)(cv->canvas_h * s);
            *out_x   = cv->base.x + (w - *out_w) / 2 + cv->pan_x;
            *out_y   = cv->base.y + (h - *out_h) / 2 + cv->pan_y;
            break;
        }

        case CANVAS_SCALE_PIXEL:
            *out_x = cv->base.x + cv->pan_x;
            *out_y = cv->base.y + cv->pan_y;
            *out_w = (int)(cv->canvas_w * cv->zoom);
            *out_h = (int)(cv->canvas_h * cv->zoom);
            break;
    }
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

Canvas *canvas_new(int x, int y, int w, int h,
                   int canvas_w, int canvas_h) {
    Canvas *cv = xmalloc(sizeof(Canvas));
    if (!cv) return NULL;

    widget_init(&cv->base, x, y, w, h);

    cv->canvas_w           = canvas_w;
    cv->canvas_h           = canvas_h;
    cv->canvas_surface     = surface_new(canvas_w, canvas_h);
    cv->scale_mode         = CANVAS_SCALE_FIT;
    cv->zoom               = 1.0f;
    cv->pan_x              = 0;
    cv->pan_y              = 0;
    cv->show_grid          = 0;
    cv->grid_size          = 16;
    cv->grid_color         = 0x33333366;
    cv->bg_color           = 0x1E1E1EFF;
    cv->checkerboard_a     = 0xCCCCCCFF;
    cv->checkerboard_b     = 0x888888FF;
    cv->show_checkerboard  = 0;

    cv->base.draw          = canvas_draw;
    cv->base.on_event      = canvas_on_event;

    if (cv->canvas_surface)
        surface_clear(cv->canvas_surface, cv->bg_color);

    return cv;
}

void canvas_free(Canvas *cv) {
    if (!cv) return;
    surface_free(cv->canvas_surface);
    widget_free_base(&cv->base);
    free(cv);
}

NvSurface *canvas_surface(Canvas *cv) {
    return cv ? cv->canvas_surface : NULL;
}

void canvas_resize(Canvas *cv, int new_w, int new_h) {
    if (!cv || new_w <= 0 || new_h <= 0) return;
    NvSurface *ns = surface_new(new_w, new_h);
    if (!ns) return;

    surface_clear(ns, cv->bg_color);
    if (cv->canvas_surface) {
        surface_blit(ns, cv->canvas_surface, 0, 0, 0, 0,
                     cv->canvas_w < new_w ? cv->canvas_w : new_w,
                     cv->canvas_h < new_h ? cv->canvas_h : new_h);
        surface_free(cv->canvas_surface);
    }

    cv->canvas_surface = ns;
    cv->canvas_w       = new_w;
    cv->canvas_h       = new_h;
    widget_set_dirty(&cv->base);
}

void canvas_clear(Canvas *cv, uint32_t color) {
    if (!cv || !cv->canvas_surface) return;
    surface_clear(cv->canvas_surface, color);
    widget_set_dirty(&cv->base);
}

void canvas_invalidate(Canvas *cv) {
    if (cv) widget_set_dirty(&cv->base);
}

/* -----------------------------------------------------------------------
 * View control
 * --------------------------------------------------------------------- */

void canvas_set_scale_mode(Canvas *cv, CanvasScaleMode mode) {
    if (!cv) return;
    cv->scale_mode = mode;
    widget_set_dirty(&cv->base);
}

void canvas_set_zoom(Canvas *cv, float zoom) {
    if (!cv) return;
    if (zoom < 0.05f) zoom = 0.05f;
    if (zoom > 64.0f) zoom = 64.0f;
    cv->zoom       = zoom;
    cv->scale_mode = CANVAS_SCALE_PIXEL;
    widget_set_dirty(&cv->base);
}

float canvas_zoom(const Canvas *cv) { return cv ? cv->zoom : 1.0f; }

void canvas_set_pan(Canvas *cv, int px, int py) {
    if (!cv) return;
    cv->pan_x = px;
    cv->pan_y = py;
    widget_set_dirty(&cv->base);
}

void canvas_reset_view(Canvas *cv) {
    if (!cv) return;
    cv->pan_x      = 0;
    cv->pan_y      = 0;
    cv->zoom       = 1.0f;
    cv->scale_mode = CANVAS_SCALE_FIT;
    widget_set_dirty(&cv->base);
}

/* -----------------------------------------------------------------------
 * Visual options
 * --------------------------------------------------------------------- */

void canvas_set_show_grid(Canvas *cv, int show, int grid_size) {
    if (!cv) return;
    cv->show_grid = show;
    cv->grid_size = grid_size > 0 ? grid_size : 16;
    widget_set_dirty(&cv->base);
}

void canvas_set_grid_color(Canvas *cv, uint32_t color) {
    if (cv) cv->grid_color = color;
}

void canvas_set_bg_color(Canvas *cv, uint32_t color) {
    if (cv) cv->bg_color = color;
}

void canvas_set_checkerboard(Canvas *cv, int show, uint32_t a, uint32_t b) {
    if (!cv) return;
    cv->show_checkerboard = show;
    cv->checkerboard_a    = a;
    cv->checkerboard_b    = b;
    widget_set_dirty(&cv->base);
}

/* -----------------------------------------------------------------------
 * Callbacks
 * --------------------------------------------------------------------- */

void canvas_set_on_paint(Canvas *cv, void (*cb)(void *, NvSurface *), void *ud) {
    if (cv) { cv->on_paint = cb; cv->canvas_ud = ud; }
}
void canvas_set_on_mouse_down(Canvas *cv, void (*cb)(void *, int, int, int), void *ud) {
    if (cv) { cv->on_mouse_down = cb; cv->canvas_ud = ud; }
}
void canvas_set_on_mouse_up(Canvas *cv, void (*cb)(void *, int, int, int), void *ud) {
    if (cv) { cv->on_mouse_up = cb; cv->canvas_ud = ud; }
}
void canvas_set_on_mouse_move(Canvas *cv, void (*cb)(void *, int, int), void *ud) {
    if (cv) { cv->on_mouse_move = cb; cv->canvas_ud = ud; }
}
void canvas_set_on_scroll(Canvas *cv, void (*cb)(void *, float), void *ud) {
    if (cv) { cv->on_scroll = cb; cv->canvas_ud = ud; }
}

/* -----------------------------------------------------------------------
 * Draw
 * --------------------------------------------------------------------- */

static void draw_checkerboard(NvSurface *dst,
                               int x, int y, int w, int h,
                               uint32_t a, uint32_t b, int cell) {
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < w; px++) {
            int tile = ((px / cell) + (py / cell)) & 1;
            draw_pixel(dst, x + px, y + py, tile ? b : a);
        }
    }
}

static void draw_grid_overlay(NvSurface *dst,
                               int dst_x, int dst_y,
                               int dst_w, int dst_h,
                               int grid_size, float zoom,
                               uint32_t color) {
    int step = (int)(grid_size * zoom);
    if (step < 2) return;

    for (int gx = 0; gx < dst_w; gx += step)
        draw_vline(dst, dst_x + gx, dst_y, dst_h, color);
    for (int gy = 0; gy < dst_h; gy += step)
        draw_hline(dst, dst_x, dst_y + gy, dst_w, color);
}

void canvas_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Canvas *cv = (Canvas *)w;

    /* widget background */
    NvRect bg = { w->x, w->y, w->w, w->h };
    draw_fill_rect(dst, &bg, 0x141414FF);
    draw_rect(dst, &bg, (w->flags & WIDGET_FOCUSED) ? WT_BORDER_FOCUS : WT_BORDER);

    if (!cv->canvas_surface) { w->flags &= ~WIDGET_DIRTY; return; }

    /* allow the owner to repaint the canvas surface first */
    if (cv->on_paint)
        cv->on_paint(cv->canvas_ud, cv->canvas_surface);

    int dx, dy, dw, dh;
    compute_dest_rect(cv, &dx, &dy, &dw, &dh);

    /* clip destination to widget bounds */
    int clip_x1 = w->x + 1, clip_y1 = w->y + 1;
    int clip_x2 = w->x + w->w - 1, clip_y2 = w->y + w->h - 1;
    int cdx = dx < clip_x1 ? clip_x1 : dx;
    int cdy = dy < clip_y1 ? clip_y1 : dy;
    int cdx2 = (dx + dw) > clip_x2 ? clip_x2 : (dx + dw);
    int cdy2 = (dy + dh) > clip_y2 ? clip_y2 : (dy + dh);
    int cdw = cdx2 - cdx;
    int cdh = cdy2 - cdy;

    if (cdw <= 0 || cdh <= 0) { w->flags &= ~WIDGET_DIRTY; return; }

    /* checkerboard under canvas (for alpha transparency feedback) */
    if (cv->show_checkerboard) {
        int cell_px = (int)(8.0f * cv->zoom);
        if (cell_px < 1) cell_px = 1;
        draw_checkerboard(dst, cdx, cdy, cdw, cdh,
                          cv->checkerboard_a, cv->checkerboard_b, cell_px);
    } else {
        NvRect canvas_area = { cdx, cdy, cdw, cdh };
        draw_fill_rect(dst, &canvas_area, cv->bg_color);
    }

    /* blit canvas pixels, scaled */
    if (dw == cv->canvas_w && dh == cv->canvas_h) {
        /* 1:1 fast path */
        int sx = cdx - dx;
        int sy = cdy - dy;
        surface_blit_alpha(dst, cv->canvas_surface,
                           cdx, cdy, sx, sy, cdw, cdh);
    } else {
        /* scaled blit — nearest-neighbour */
        for (int py = 0; py < cdh; py++) {
            int sy = ((cdy - dy + py) * cv->canvas_h) / dh;
            if (sy < 0 || sy >= cv->canvas_h) continue;
            for (int px = 0; px < cdw; px++) {
                int sx = ((cdx - dx + px) * cv->canvas_w) / dw;
                if (sx < 0 || sx >= cv->canvas_w) continue;
                uint32_t c = cv->canvas_surface->pixels[
                    sy * cv->canvas_surface->pitch + sx];
                uint32_t *dp = &dst->pixels[
                    (cdy + py) * dst->pitch + (cdx + px)];
                *dp = color_blend(*dp, c);
            }
        }
    }

    /* grid overlay */
    if (cv->show_grid && cv->zoom >= 2.0f) {
        draw_grid_overlay(dst, cdx, cdy, cdw, cdh,
                          cv->grid_size, cv->zoom, cv->grid_color);
    }

    /* canvas border */
    NvRect canvas_border = { dx - 1, dy - 1, dw + 2, dh + 2 };
    draw_rect(dst, &canvas_border, 0x333333FF);

    w->flags &= ~WIDGET_DIRTY;
}

/* -----------------------------------------------------------------------
 * Events
 * --------------------------------------------------------------------- */

void canvas_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Canvas *cv = (Canvas *)w;
    (void)ud;

    switch (ev->type) {
        case WIDGET_EV_MOUSE_DOWN: {
            int cx = canvas_to_surface_x(cv, ev->mouse.x);
            int cy = canvas_to_surface_y(cv, ev->mouse.y);

            if (ev->mouse.button == 2 || (ev->mouse.button == 1 && (ev->mouse.mods & 0x04))) {
                /* middle mouse / alt+left: pan */
                cv->panning      = 1;
                cv->pan_start_x  = ev->mouse.x;
                cv->pan_start_y  = ev->mouse.y;
                cv->pan_start_px = cv->pan_x;
                cv->pan_start_py = cv->pan_y;
            } else if (cv->on_mouse_down) {
                cv->on_mouse_down(cv->canvas_ud, cx, cy, ev->mouse.button);
            }
            break;
        }

        case WIDGET_EV_MOUSE_UP:
            cv->panning = 0;
            if (cv->on_mouse_up) {
                int cx = canvas_to_surface_x(cv, ev->mouse.x);
                int cy = canvas_to_surface_y(cv, ev->mouse.y);
                cv->on_mouse_up(cv->canvas_ud, cx, cy, ev->mouse.button);
            }
            break;

        case WIDGET_EV_MOUSE_MOVE:
            if (cv->panning) {
                cv->pan_x = cv->pan_start_px + (ev->mouse.x - cv->pan_start_x);
                cv->pan_y = cv->pan_start_py + (ev->mouse.y - cv->pan_start_y);
                widget_set_dirty(w);
            } else if (cv->on_mouse_move) {
                int cx = canvas_to_surface_x(cv, ev->mouse.x);
                int cy = canvas_to_surface_y(cv, ev->mouse.y);
                cv->on_mouse_move(cv->canvas_ud, cx, cy);
            }
            break;

        case WIDGET_EV_SCROLL: {
            int dy = ev->scroll.dy;
            if (ev->scroll.dy != 0 && (w->flags & WIDGET_FOCUSED)) {
                if (cv->scale_mode == CANVAS_SCALE_PIXEL) {
                    /* zoom in/out centred on cursor */
                    int mx = ev->mouse.x;
                    int my = ev->mouse.y;
                    float old_zoom = cv->zoom;
                    float new_zoom = old_zoom * (dy < 0 ? 1.1f : 0.9f);
                    if (new_zoom < 0.1f) new_zoom = 0.1f;
                    if (new_zoom > 32.0f) new_zoom = 32.0f;

                    /* adjust pan so the point under the cursor stays fixed */
                    cv->pan_x = mx - (int)((mx - cv->pan_x) * new_zoom / old_zoom);
                    cv->pan_y = my - (int)((my - cv->pan_y) * new_zoom / old_zoom);
                    cv->zoom  = new_zoom;

                    if (cv->on_scroll) cv->on_scroll(cv->canvas_ud, new_zoom);
                    widget_set_dirty(w);
                } else {
                    cv->pan_y += dy * 20;
                    widget_set_dirty(w);
                }
            }
            break;
        }

        case WIDGET_EV_FOCUS_IN:
        case WIDGET_EV_FOCUS_OUT:
            widget_set_dirty(w);
            break;

        default:
            break;
    }
}