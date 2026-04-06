#include "window.h"
#include "wm.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"

#define SHADOW_STEPS   6
#define SHADOW_BASE_A  0x55

void window_draw_shadow(NvSurface *surface, int x, int y, int w, int h,
                         int focused) {
    if (!surface) return;
    int alpha_base = focused ? SHADOW_BASE_A : SHADOW_BASE_A / 2;
    for (int i = SHADOW_STEPS; i >= 1; i--) {
        int alpha = alpha_base / i;
        NvRect r = { x + i, y + i, w, h };
        draw_fill_rect_alpha(surface, &r,
                             (uint32_t)(0x00000000 | alpha));
    }
}

void window_draw_border(NvSurface *surface, int x, int y, int w, int h,
                         int focused, int border_w) {
    if (!surface) return;
    uint32_t border_color = focused ? 0x007ACCFF : 0x3F3F46FF;
    for (int i = 0; i < border_w; i++) {
        NvRect r = { x + i, y + i, w - i*2, h - i*2 };
        draw_rect(surface, &r, border_color);
    }
}

void window_draw_resize_handles(NvSurface *surface, int x, int y,
                                 int w, int h) {
    if (!surface) return;
    uint32_t handle_color = 0x007ACC88;
    int s = WM_RESIZE_HANDLE;
    NvRect corners[] = {
        { x,           y,           s, s },
        { x + w - s,   y,           s, s },
        { x,           y + h - s,   s, s },
        { x + w - s,   y + h - s,   s, s },
    };
    for (int i = 0; i < 4; i++)
        draw_fill_rect_alpha(surface, &corners[i], handle_color);
}

void window_draw_frame(NvSurface *surface, int w, int h,
                        const char *title, int focused,
                        int maximized, int flags) {
    if (!surface) return;

    uint32_t bg = focused ? 0x1E1E1EFF : 0x252526FF;
    uint32_t border = focused ? 0x007ACCFF : 0x3F3F46FF;

    NvRect full = { 0, 0, w, h };
    draw_fill_rect(surface, &full, bg);

    if (!maximized) {
        draw_rect(surface, &full, border);
        if (flags & WIN_FLAG_RESIZABLE)
            window_draw_resize_handles(surface, 0, 0, w, h);
    }

    /* titlebar background */
    NvRect tb = { WM_BORDER_W, 0, w - WM_BORDER_W*2, WM_TITLEBAR_H };
    uint32_t tb_color = focused ? 0x007ACC20 : 0x1E1E1E00;
    draw_fill_rect_alpha(surface, &tb, tb_color);

    /* titlebar bottom border line */
    NvRect tb_line = { WM_BORDER_W, WM_TITLEBAR_H - 1,
                       w - WM_BORDER_W*2, 1 };
    draw_fill_rect(surface, &tb_line,
                   focused ? 0x007ACCFF : 0x3F3F46FF);

    if (title && (flags & WIN_FLAG_DECORATED)) {
        int tw = (int)strlen(title) * 7;
        int tx = (w - tw) / 2;
        int ty = (WM_TITLEBAR_H - 13) / 2;
        if (tx < WM_TITLEBAR_H) tx = WM_TITLEBAR_H;
        uint32_t fg = focused ? 0xCCCCCCFF : 0x666666FF;
        font_draw_string_clip(surface, tx, ty, title, fg, 13,
                              w - WM_TITLEBAR_H*3 - WM_BORDER_W*2);
    }
}

void window_draw_client_bg(NvSurface *surface, int cx, int cy,
                             int cw, int ch) {
    if (!surface) return;
    NvRect r = { cx, cy, cw, ch };
    draw_fill_rect(surface, &r, 0x1E1E1EFF);
}