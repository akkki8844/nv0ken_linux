#ifndef NV0_DRAW_H
#define NV0_DRAW_H

#include <stdint.h>
#include "../../../graphics/draw/draw.h"
#include "fs.h"

typedef uint32_t NvColor;

#define NV_COLOR(r, g, b, a) \
    ((((uint32_t)(a) & 0xffu) << 24) | \
     (((uint32_t)(r) & 0xffu) << 16) | \
     (((uint32_t)(g) & 0xffu) << 8)  | \
      ((uint32_t)(b) & 0xffu))

void nv_draw_pixel(NvSurface *surface, int x, int y, NvColor color);
void nv_draw_fill_rect(NvSurface *surface, const NvRect *rect, NvColor color);
void nv_draw_rect(NvSurface *surface, const NvRect *rect, NvColor color);
void nv_draw_text(NvSurface *surface, int x, int y,
                  const char *text, NvColor color);
void nv_draw_text_sized(NvSurface *surface, int x, int y,
                        const char *text, NvColor color, int size);
void nv_draw_text_clip(NvSurface *surface, int x, int y,
                       const char *text, NvColor color, int max_width);
void nv_draw_text_styled(NvSurface *surface, int x, int y,
                         const char *text, NvColor color, int size,
                         int bold, int underline);
int  nv_font_measure(const char *text, int size);
void nv_font_draw(NvSurface *surface, int x, int y,
                  const char *text, NvColor color, int size);

void nv_surface_push_clip(NvSurface *surface, const NvRect *rect);
void nv_surface_pop_clip(NvSurface *surface);
void nv_surface_invalidate(NvSurface *surface);

#endif
