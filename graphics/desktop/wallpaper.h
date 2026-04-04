#ifndef WALLPAPER_H
#define WALLPAPER_H

#include "../draw/draw.h"
#include <stdint.h>

typedef enum {
    WALLPAPER_STRETCH,
    WALLPAPER_FIT,
    WALLPAPER_FILL,
    WALLPAPER_CENTER,
    WALLPAPER_TILE,
} WallpaperMode;

void wallpaper_draw(NvSurface *surface,
                    const uint32_t *pixels, int img_w, int img_h,
                    int dst_x, int dst_y, int dst_w, int dst_h,
                    WallpaperMode mode);

void wallpaper_draw_gradient(NvSurface *surface,
                             int x, int y, int w, int h,
                             uint32_t top_color, uint32_t bottom_color);

#endif