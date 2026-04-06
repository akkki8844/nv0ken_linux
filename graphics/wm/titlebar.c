#include "titlebar.h"
#include "wm.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Button geometry
 * button order right-to-left: close, maximize, minimize
 * -------------------------------------------------------------------- */

void titlebar_get_close_rect(int win_w, int win_h,
                               int *x, int *y, int *w, int *h) {
    (void)win_h;
    *w = BTN_RADIUS * 2;
    *h = BTN_RADIUS * 2;
    *x = win_w - WM_BORDER_W - BTN_MARGIN - *w;
    *y = BTN_Y_OFFSET;
}

void titlebar_get_max_rect(int win_w, int win_h,
                             int *x, int *y, int *w, int *h) {
    int cx, cy, cw, ch;
    titlebar_get_close_rect(win_w, win_h, &cx, &cy, &cw, &ch);
    *w = BTN_RADIUS * 2;
    *h = BTN_RADIUS * 2;
    *x = cx - BTN_MARGIN - *w;
    *y = BTN_Y_OFFSET;
}

void titlebar_get_min_rect(int win_w, int win_h,
                             int *x, int *y, int *w, int *h) {
    int mx, my, mw, mh;
    titlebar_get_max_rect(win_w, win_h, &mx, &my, &mw, &mh);
    *w = BTN_RADIUS * 2;
    *h = BTN_RADIUS * 2;
    *x = mx - BTN_MARGIN - *w;
    *y = BTN_Y_OFFSET;
}

int titlebar_hit_close(int win_w, int win_h, int x, int y) {
    int bx, by, bw, bh;
    titlebar_get_close_rect(win_w, win_h, &bx, &by, &bw, &bh);
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

int titlebar_hit_maximize(int win_w, int win_h, int x, int y) {
    int bx, by, bw, bh;
    titlebar_get_max_rect(win_w, win_h, &bx, &by, &bw, &bh);
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

int titlebar_hit_minimize(int win_w, int win_h, int x, int y) {
    int bx, by, bw, bh;
    titlebar_get_min_rect(win_w, win_h, &bx, &by, &bw, &bh);
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_btn(NvSurface *surface, int x, int y, int r,
                      uint32_t color, const char *symbol,
                      int hovered) {
    draw_fill_circle(surface, x + r, y + r, r, color);
    if (hovered) {
        draw_circle(surface, x + r, y + r, r,
                    color_darken(color, 40));
        if (symbol) {
            uint32_t fg = color_darken(color, 80);
            font_draw_string(surface,
                             x + r - (int)strlen(symbol) * 3,
                             y + r - 5,
                             symbol, fg, 10);
        }
    }
}

void titlebar_draw(NvSurface *surface, int win_w, int win_h,
                   const char *title, int focused,
                   int maximized, int flags) {
    if (!surface) return;

    /* background */
    uint32_t tb_bg = focused ? 0x252526FF : 0x1E1E1EFF;
    NvRect tb_rect = { WM_BORDER_W, 0,
                       win_w - WM_BORDER_W * 2, WM_TITLEBAR_H };
    draw_fill_rect(surface, &tb_rect, tb_bg);

    /* bottom separator */
    NvRect sep = { WM_BORDER_W, WM_TITLEBAR_H - 1,
                   win_w - WM_BORDER_W * 2, 1 };
    draw_fill_rect(surface, &sep, focused ? 0x007ACCFF : 0x3F3F46FF);

    /* gradient overlay */
    NvRect grad_r = { WM_BORDER_W, 0,
                      win_w - WM_BORDER_W * 2, WM_TITLEBAR_H - 1 };
    draw_gradient_v(surface, &grad_r,
                    focused ? 0x007ACC18 : 0x00000010,
                    0x00000000);

    /* title text */
    if (title && (flags & WIN_FLAG_DECORATED)) {
        int tw, th;
        font_measure_string(title, 13, &tw, &th);
        int max_tw = win_w - (BTN_RADIUS*2 + BTN_MARGIN) * 4 - WM_BORDER_W*2;
        int tx = WM_BORDER_W + 8;
        int ty = (WM_TITLEBAR_H - 13) / 2;
        uint32_t fg = focused ? 0xCCCCCCFF : 0x555555FF;
        font_draw_string_clip(surface, tx, ty, title, fg, 13, max_tw);
    }

    /* window control buttons */
    if (!(flags & WIN_FLAG_DECORATED)) return;

    int cx, cy, cw, ch;
    titlebar_get_close_rect(win_w, win_h, &cx, &cy, &cw, &ch);
    draw_btn(surface, cx, cy, BTN_RADIUS, BTN_CLOSE_COLOR,
             "x", focused);

    int mx, my, mw, mh;
    titlebar_get_max_rect(win_w, win_h, &mx, &my, &mw, &mh);
    const char *max_sym = maximized ? "-" : "+";
    draw_btn(surface, mx, my, BTN_RADIUS, BTN_MAX_COLOR,
             max_sym, focused);

    int minx, miny, minw, minh;
    titlebar_get_min_rect(win_w, win_h, &minx, &miny, &minw, &minh);
    draw_btn(surface, minx, miny, BTN_RADIUS, BTN_MIN_COLOR,
             "_", focused);

    /* border */
    if (!maximized) {
        uint32_t border = focused ? 0x007ACCFF : 0x3F3F46FF;
        NvRect frame = { 0, 0, win_w, win_h };
        draw_rect(surface, &frame, border);
    }
}