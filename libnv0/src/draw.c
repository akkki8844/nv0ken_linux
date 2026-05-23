#include "../include/nv0/draw.h"

#include "../../graphics/font/font_render.h"
#include "internal.h"

#define CLIP_STACK_MAX 32

typedef struct {
    NvSurface *surface;
    NvRect rect;
} ClipEntry;

static ClipEntry clip_stack[CLIP_STACK_MAX];
static int clip_depth;

static NvRect surface_bounds(NvSurface *surface)
{
    NvRect bounds = {0, 0, surface ? surface->width : 0, surface ? surface->height : 0};
    return bounds;
}

static NvRect current_clip(NvSurface *surface)
{
    NvRect clip = surface_bounds(surface);
    for (int i = 0; i < clip_depth; ++i) {
        if (clip_stack[i].surface == surface) {
            clip = rect_intersect(&clip, &clip_stack[i].rect);
        }
    }
    return clip;
}

void nv_draw_pixel(NvSurface *surface, int x, int y, NvColor color)
{
    NvRect clip = current_clip(surface);
    if (!rect_contains(&clip, x, y)) {
        return;
    }
    draw_pixel(surface, x, y, color);
}

void nv_draw_fill_rect(NvSurface *surface, const NvRect *rect, NvColor color)
{
    if (!surface || !rect) {
        return;
    }
    NvRect clip = current_clip(surface);
    NvRect clipped = rect_intersect(rect, &clip);
    draw_fill_rect(surface, &clipped, color);
}

void nv_draw_rect(NvSurface *surface, const NvRect *rect, NvColor color)
{
    if (!surface || !rect) {
        return;
    }
    NvRect top = {rect->x, rect->y, rect->w, 1};
    NvRect bottom = {rect->x, rect->y + rect->h - 1, rect->w, 1};
    NvRect left = {rect->x, rect->y, 1, rect->h};
    NvRect right = {rect->x + rect->w - 1, rect->y, 1, rect->h};
    nv_draw_fill_rect(surface, &top, color);
    nv_draw_fill_rect(surface, &bottom, color);
    nv_draw_fill_rect(surface, &left, color);
    nv_draw_fill_rect(surface, &right, color);
}

void nv_draw_text(NvSurface *surface, int x, int y, const char *text, NvColor color)
{
    nv_draw_text_sized(surface, x, y, text, color, 13);
}

void nv_draw_text_sized(NvSurface *surface, int x, int y,
                        const char *text, NvColor color, int size)
{
    NvRect clip = current_clip(surface);
    if (rect_empty(&clip)) {
        return;
    }
    (void)clip;
    font_draw_string(surface, x, y, text, color, size);
}

void nv_draw_text_clip(NvSurface *surface, int x, int y,
                       const char *text, NvColor color, int max_width)
{
    NvRect clip = current_clip(surface);
    NvRect text_clip = {x, y, max_width, surface ? surface->height - y : 0};
    NvRect combined = rect_intersect(&clip, &text_clip);
    nv_surface_push_clip(surface, &combined);
    font_draw_string_clip(surface, x, y, text, color, 13, max_width);
    nv_surface_pop_clip(surface);
}

void nv_draw_text_styled(NvSurface *surface, int x, int y,
                         const char *text, NvColor color, int size,
                         int bold, int underline)
{
    font_draw_string_styled(surface, x, y, text, color, 0, size, bold, 0, underline);
}

int nv_font_measure(const char *text, int size)
{
    int width = 0;
    int height = 0;
    font_measure_string(text, size, &width, &height);
    return width;
}

void nv_font_draw(NvSurface *surface, int x, int y,
                  const char *text, NvColor color, int size)
{
    nv_draw_text_sized(surface, x, y, text, color, size);
}

void nv_surface_push_clip(NvSurface *surface, const NvRect *rect)
{
    if (!surface || !rect || clip_depth >= CLIP_STACK_MAX) {
        return;
    }
    clip_stack[clip_depth].surface = surface;
    clip_stack[clip_depth].rect = *rect;
    ++clip_depth;
}

void nv_surface_pop_clip(NvSurface *surface)
{
    for (int i = clip_depth - 1; i >= 0; --i) {
        if (clip_stack[i].surface == surface) {
            for (int j = i; j + 1 < clip_depth; ++j) {
                clip_stack[j] = clip_stack[j + 1];
            }
            --clip_depth;
            return;
        }
    }
}

void nv_surface_invalidate(NvSurface *surface)
{
    for (int i = 0; i < nv_window_count; ++i) {
        if (nv_windows[i] && nv_windows[i]->surface == surface) {
            nv_window_mark_dirty(nv_windows[i]);
            return;
        }
    }
}
