#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * ARGB32 pack/unpack
 * pixel format: 0xAARRGGBB
 * --------------------------------------------------------------------- */

#define COLOR_A(c)  (((c) >> 24) & 0xFF)
#define COLOR_R(c)  (((c) >> 16) & 0xFF)
#define COLOR_G(c)  (((c) >>  8) & 0xFF)
#define COLOR_B(c)  (((c)      ) & 0xFF)

#define COLOR_ARGB(a,r,g,b) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) | \
      (uint32_t)(b))

#define COLOR_RGB(r,g,b)  COLOR_ARGB(0xFF,(r),(g),(b))
#define COLOR_RGBA(r,g,b,a) COLOR_ARGB((a),(r),(g),(b))

/* Common colors */
#define COLOR_WHITE       0xFFFFFFFF
#define COLOR_BLACK       0xFF000000
#define COLOR_TRANSPARENT 0x00000000
#define COLOR_RED         0xFFFF0000
#define COLOR_GREEN       0xFF00FF00
#define COLOR_BLUE        0xFF0000FF
#define COLOR_YELLOW      0xFFFFFF00
#define COLOR_CYAN        0xFF00FFFF
#define COLOR_MAGENTA     0xFFFF00FF
#define COLOR_GRAY        0xFF808080
#define COLOR_DARK_GRAY   0xFF404040
#define COLOR_LIGHT_GRAY  0xFFD0D0D0

static inline uint32_t color_with_alpha(uint32_t color, uint8_t alpha) {
    return (color & 0x00FFFFFF) | ((uint32_t)alpha << 24);
}

static inline uint32_t color_lerp(uint32_t a, uint32_t b, uint8_t t) {
    uint8_t ra = COLOR_R(a), ga = COLOR_G(a), ba = COLOR_B(a), aa = COLOR_A(a);
    uint8_t rb = COLOR_R(b), gb = COLOR_G(b), bb = COLOR_B(b), ab = COLOR_A(b);
    uint8_t r = (uint8_t)(ra + ((rb - ra) * t) / 255);
    uint8_t g = (uint8_t)(ga + ((gb - ga) * t) / 255);
    uint8_t bv= (uint8_t)(ba + ((bb - ba) * t) / 255);
    uint8_t av= (uint8_t)(aa + ((ab - aa) * t) / 255);
    return COLOR_ARGB(av, r, g, bv);
}

static inline uint32_t color_blend(uint32_t dst, uint32_t src) {
    uint32_t sa = COLOR_A(src);
    if (sa == 0)   return dst;
    if (sa == 255) return src;
    uint32_t da  = 255 - sa;
    uint8_t  r   = (uint8_t)((COLOR_R(src) * sa + COLOR_R(dst) * da) / 255);
    uint8_t  g   = (uint8_t)((COLOR_G(src) * sa + COLOR_G(dst) * da) / 255);
    uint8_t  b   = (uint8_t)((COLOR_B(src) * sa + COLOR_B(dst) * da) / 255);
    uint8_t  a   = (uint8_t)(sa + (COLOR_A(dst) * da) / 255);
    return COLOR_ARGB(a, r, g, b);
}

static inline uint32_t color_premul_blend(uint32_t dst, uint32_t src) {
    uint32_t sa = COLOR_A(src);
    if (sa == 0)   return dst;
    if (sa == 255) return (src & 0x00FFFFFF) | 0xFF000000;
    uint32_t inv = 255 - sa;
    uint8_t  r   = (uint8_t)(COLOR_R(src) + (COLOR_R(dst) * inv) / 255);
    uint8_t  g   = (uint8_t)(COLOR_G(src) + (COLOR_G(dst) * inv) / 255);
    uint8_t  b   = (uint8_t)(COLOR_B(src) + (COLOR_B(dst) * inv) / 255);
    return COLOR_ARGB(0xFF, r, g, b);
}

static inline uint32_t color_lighten(uint32_t c, int amount) {
    int r = COLOR_R(c) + amount; if (r > 255) r = 255;
    int g = COLOR_G(c) + amount; if (g > 255) g = 255;
    int b = COLOR_B(c) + amount; if (b > 255) b = 255;
    return COLOR_ARGB(COLOR_A(c), r, g, b);
}

static inline uint32_t color_darken(uint32_t c, int amount) {
    int r = COLOR_R(c) - amount; if (r < 0) r = 0;
    int g = COLOR_G(c) - amount; if (g < 0) g = 0;
    int b = COLOR_B(c) - amount; if (b < 0) b = 0;
    return COLOR_ARGB(COLOR_A(c), r, g, b);
}

static inline uint32_t color_grayscale(uint32_t c) {
    uint8_t y = (uint8_t)(COLOR_R(c) * 299 / 1000 +
                          COLOR_G(c) * 587 / 1000 +
                          COLOR_B(c) * 114 / 1000);
    return COLOR_ARGB(COLOR_A(c), y, y, y);
}

static inline uint32_t color_invert(uint32_t c) {
    return COLOR_ARGB(COLOR_A(c),
                      255 - COLOR_R(c),
                      255 - COLOR_G(c),
                      255 - COLOR_B(c));
}

static inline uint32_t color_multiply(uint32_t a, uint32_t b) {
    return COLOR_ARGB(
        (COLOR_A(a) * COLOR_A(b)) / 255,
        (COLOR_R(a) * COLOR_R(b)) / 255,
        (COLOR_G(a) * COLOR_G(b)) / 255,
        (COLOR_B(a) * COLOR_B(b)) / 255);
}

static inline uint32_t color_screen(uint32_t a, uint32_t b) {
    return COLOR_ARGB(
        255 - ((255 - COLOR_A(a)) * (255 - COLOR_A(b))) / 255,
        255 - ((255 - COLOR_R(a)) * (255 - COLOR_R(b))) / 255,
        255 - ((255 - COLOR_G(a)) * (255 - COLOR_G(b))) / 255,
        255 - ((255 - COLOR_B(a)) * (255 - COLOR_B(b))) / 255);
}

static inline uint32_t color_set_alpha(uint32_t c, uint8_t a) {
    return (c & 0x00FFFFFF) | ((uint32_t)a << 24);
}

static inline int color_luminance(uint32_t c) {
    return (COLOR_R(c) * 299 + COLOR_G(c) * 587 + COLOR_B(c) * 114) / 1000;
}

static inline uint32_t color_contrast_fg(uint32_t bg) {
    return color_luminance(bg) > 128 ? COLOR_BLACK : COLOR_WHITE;
}

#endif