#include "wallpaper.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

static inline uint32_t sample_bilinear(const uint32_t *pixels,
                                        int img_w, int img_h,
                                        float u, float v) {
    if (u < 0.0f) u = 0.0f;
    if (v < 0.0f) v = 0.0f;
    if (u > 1.0f) u = 1.0f;
    if (v > 1.0f) v = 1.0f;

    float fx = u * (img_w - 1);
    float fy = v * (img_h - 1);
    int   x0 = (int)fx, y0 = (int)fy;
    int   x1 = x0 + 1 < img_w ? x0 + 1 : x0;
    int   y1 = y0 + 1 < img_h ? y0 + 1 : y0;
    float tx = fx - x0, ty = fy - y0;

    uint32_t c00 = pixels[y0 * img_w + x0];
    uint32_t c10 = pixels[y0 * img_w + x1];
    uint32_t c01 = pixels[y1 * img_w + x0];
    uint32_t c11 = pixels[y1 * img_w + x1];

    uint8_t r = (uint8_t)(
        ((c00 >> 16 & 0xFF) * (1-tx) * (1-ty)) +
        ((c10 >> 16 & 0xFF) * tx     * (1-ty)) +
        ((c01 >> 16 & 0xFF) * (1-tx) * ty    ) +
        ((c11 >> 16 & 0xFF) * tx     * ty    ));
    uint8_t g = (uint8_t)(
        ((c00 >> 8  & 0xFF) * (1-tx) * (1-ty)) +
        ((c10 >> 8  & 0xFF) * tx     * (1-ty)) +
        ((c01 >> 8  & 0xFF) * (1-tx) * ty    ) +
        ((c11 >> 8  & 0xFF) * tx     * ty    ));
    uint8_t b = (uint8_t)(
        ((c00       & 0xFF) * (1-tx) * (1-ty)) +
        ((c10       & 0xFF) * tx     * (1-ty)) +
        ((c01       & 0xFF) * (1-tx) * ty    ) +
        ((c11       & 0xFF) * tx     * ty    ));

    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void draw_scaled(NvSurface *surface,
                         const uint32_t *pixels, int img_w, int img_h,
                         int dst_x, int dst_y, int dst_w, int dst_h,
                         int src_x, int src_y, int src_w, int src_h) {
    for (int py = 0; py < dst_h; py++) {
        float v = (float)(src_y + py * src_h / dst_h) / (float)img_h;
        for (int px = 0; px < dst_w; px++) {
            float u = (float)(src_x + px * src_w / dst_w) / (float)img_w;
            uint32_t color = sample_bilinear(pixels, img_w, img_h, u, v);
            draw_pixel(surface, dst_x + px, dst_y + py, color);
        }
    }
}

void wallpaper_draw(NvSurface *surface,
                    const uint32_t *pixels, int img_w, int img_h,
                    int dst_x, int dst_y, int dst_w, int dst_h,
                    WallpaperMode mode) {
    if (!surface || !pixels || img_w <= 0 || img_h <= 0) return;

    NvRect bg = { dst_x, dst_y, dst_w, dst_h };

    switch (mode) {
        case WALLPAPER_STRETCH:
            draw_scaled(surface, pixels, img_w, img_h,
                        dst_x, dst_y, dst_w, dst_h,
                        0, 0, img_w, img_h);
            break;

        case WALLPAPER_FIT: {
            float scale_x = (float)dst_w / img_w;
            float scale_y = (float)dst_h / img_h;
            float scale   = scale_x < scale_y ? scale_x : scale_y;
            int   sw = (int)(img_w * scale);
            int   sh = (int)(img_h * scale);
            int   ox = dst_x + (dst_w - sw) / 2;
            int   oy = dst_y + (dst_h - sh) / 2;

            draw_fill_rect(surface, &bg, 0x000000FF);
            draw_scaled(surface, pixels, img_w, img_h,
                        ox, oy, sw, sh, 0, 0, img_w, img_h);
            break;
        }

        case WALLPAPER_FILL: {
            float scale_x = (float)dst_w / img_w;
            float scale_y = (float)dst_h / img_h;
            float scale   = scale_x > scale_y ? scale_x : scale_y;
            int   sw = (int)(img_w * scale);
            int   sh = (int)(img_h * scale);
            int   cx = (sw - dst_w) / 2;
            int   cy = (sh - dst_h) / 2;

            for (int py = 0; py < dst_h; py++) {
                for (int px = 0; px < dst_w; px++) {
                    float u = (float)(cx + px) / sw;
                    float v = (float)(cy + py) / sh;
                    uint32_t c = sample_bilinear(pixels, img_w, img_h, u, v);
                    draw_pixel(surface, dst_x + px, dst_y + py, c);
                }
            }
            break;
        }

        case WALLPAPER_CENTER: {
            draw_fill_rect(surface, &bg, 0x1A1A2EFF);
            int ox = dst_x + (dst_w - img_w) / 2;
            int oy = dst_y + (dst_h - img_h) / 2;
            for (int py = 0; py < img_h; py++) {
                int dy = oy + py;
                if (dy < dst_y || dy >= dst_y + dst_h) continue;
                for (int px = 0; px < img_w; px++) {
                    int dx = ox + px;
                    if (dx < dst_x || dx >= dst_x + dst_w) continue;
                    draw_pixel(surface, dx, dy,
                               pixels[py * img_w + px]);
                }
            }
            break;
        }

        case WALLPAPER_TILE: {
            for (int ty = dst_y; ty < dst_y + dst_h; ty += img_h) {
                for (int tx = dst_x; tx < dst_x + dst_w; tx += img_w) {
                    for (int py = 0; py < img_h; py++) {
                        if (ty + py >= dst_y + dst_h) break;
                        for (int px = 0; px < img_w; px++) {
                            if (tx + px >= dst_x + dst_w) break;
                            draw_pixel(surface, tx + px, ty + py,
                                       pixels[py * img_w + px]);
                        }
                    }
                }
            }
            break;
        }
    }
}

void wallpaper_draw_gradient(NvSurface *surface,
                              int x, int y, int w, int h,
                              uint32_t top_color, uint32_t bottom_color) {
    if (!surface) return;

    uint8_t tr = (top_color    >> 24) & 0xFF;
    uint8_t tg = (top_color    >> 16) & 0xFF;
    uint8_t tb = (top_color    >>  8) & 0xFF;
    uint8_t br = (bottom_color >> 24) & 0xFF;
    uint8_t bg = (bottom_color >> 16) & 0xFF;
    uint8_t bb = (bottom_color >>  8) & 0xFF;

    for (int py = 0; py < h; py++) {
        float t = (float)py / (float)(h - 1);
        uint8_t r = (uint8_t)(tr + (br - tr) * t);
        uint8_t g = (uint8_t)(tg + (bg - tg) * t);
        uint8_t b = (uint8_t)(tb + (bb - tb) * t);
        uint32_t color = 0xFF000000 | ((uint32_t)r << 16) |
                         ((uint32_t)g << 8) | b;
        NvRect row = { x, y + py, w, 1 };
        draw_fill_rect(surface, &row, color);
    }
}