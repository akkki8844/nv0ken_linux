#ifndef CANVAS_H
#define CANVAS_H

#include "widget.h"
#include "../draw/draw.h"

/* -----------------------------------------------------------------------
 * Canvas — a widget that owns a pixel surface and exposes it for
 * arbitrary drawing. Supports mouse events with coordinate translation,
 * invalidation, resize, and an optional grid overlay.
 * --------------------------------------------------------------------- */

typedef enum {
    CANVAS_SCALE_NONE,
    CANVAS_SCALE_FIT,
    CANVAS_SCALE_FILL,
    CANVAS_SCALE_PIXEL,
} CanvasScaleMode;

typedef struct {
    Widget        base;

    NvSurface    *canvas_surface;
    int           canvas_w;
    int           canvas_h;

    CanvasScaleMode scale_mode;
    float         zoom;
    int           pan_x;
    int           pan_y;

    int           show_grid;
    int           grid_size;
    uint32_t      grid_color;
    uint32_t      bg_color;
    uint32_t      checkerboard_a;
    uint32_t      checkerboard_b;
    int           show_checkerboard;

    int           panning;
    int           pan_start_x;
    int           pan_start_y;
    int           pan_start_px;
    int           pan_start_py;

    void (*on_paint)(void *ud, NvSurface *surface);
    void (*on_mouse_down)(void *ud, int cx, int cy, int button);
    void (*on_mouse_up)(void *ud, int cx, int cy, int button);
    void (*on_mouse_move)(void *ud, int cx, int cy);
    void (*on_scroll)(void *ud, float delta);
    void *canvas_ud;
} Canvas;

Canvas    *canvas_new(int x, int y, int w, int h,
                       int canvas_w, int canvas_h);
void       canvas_free(Canvas *cv);

NvSurface *canvas_surface(Canvas *cv);
void       canvas_resize(Canvas *cv, int new_w, int new_h);
void       canvas_clear(Canvas *cv, uint32_t color);
void       canvas_invalidate(Canvas *cv);

void       canvas_set_scale_mode(Canvas *cv, CanvasScaleMode mode);
void       canvas_set_zoom(Canvas *cv, float zoom);
float      canvas_zoom(const Canvas *cv);
void       canvas_set_pan(Canvas *cv, int px, int py);
void       canvas_reset_view(Canvas *cv);

int        canvas_to_surface_x(const Canvas *cv, int widget_x);
int        canvas_to_surface_y(const Canvas *cv, int widget_y);
int        canvas_from_surface_x(const Canvas *cv, int surface_x);
int        canvas_from_surface_y(const Canvas *cv, int surface_y);

void       canvas_set_show_grid(Canvas *cv, int show, int grid_size);
void       canvas_set_grid_color(Canvas *cv, uint32_t color);
void       canvas_set_bg_color(Canvas *cv, uint32_t color);
void       canvas_set_checkerboard(Canvas *cv, int show,
                                    uint32_t a, uint32_t b);

void       canvas_set_on_paint(Canvas *cv, void (*cb)(void *, NvSurface *), void *ud);
void       canvas_set_on_mouse_down(Canvas *cv, void (*cb)(void *, int, int, int), void *ud);
void       canvas_set_on_mouse_up(Canvas *cv, void (*cb)(void *, int, int, int), void *ud);
void       canvas_set_on_mouse_move(Canvas *cv, void (*cb)(void *, int, int), void *ud);
void       canvas_set_on_scroll(Canvas *cv, void (*cb)(void *, float), void *ud);

void       canvas_draw(Widget *w, NvSurface *dst);
void       canvas_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif