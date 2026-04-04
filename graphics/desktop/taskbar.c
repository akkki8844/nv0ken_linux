#include "taskbar.h"
#include "launcher.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BG_COLOR          0x252526FF
#define BG_COLOR_TOP      0x2D2D30FF
#define BORDER_COLOR      0x3F3F46FF
#define BTN_NORMAL        0x3F3F46FF
#define BTN_FOCUSED       0x007ACCFF
#define BTN_HOVERED       0x505050FF
#define BTN_MINIMIZED     0x2A2A2AFF
#define BTN_TEXT_NORMAL   0xCCCCCCFF
#define BTN_TEXT_FOCUSED  0xFFFFFFFF
#define START_BG          0x007ACCFF
#define START_HOVER       0x1B8AD3FF
#define CLOCK_COLOR       0xCCCCCCFF
#define TRAY_ACTIVE       0x4FC3F7FF
#define TRAY_INACTIVE     0x555555FF
#define SEPARATOR_COLOR   0x444444FF
#define HIGHLIGHT_LINE    0x007ACCFF

struct Taskbar {
    Compositor    *comp;
    WindowManager *wm;

    int            x, y, w, h;

    TaskBtn        btns[TASKBAR_MAX_BTNS];
    int            btn_count;
    int            hovered_btn;

    TrayItem       tray[TASKBAR_MAX_TRAY];
    int            tray_count;
    int            hovered_tray;

    int            start_hovered;
    int            start_pressed;
    int            launcher_open;

    char           clock_str[16];
    char           date_str[16];
    int            tick_count;

    int            needs_redraw;
};

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

static void update_clock(Taskbar *t) {
    long long ts;
    sys_time(&ts);

    long long s  = ts % 60; ts /= 60;
    long long m  = ts % 60; ts /= 60;
    long long h  = ts % 24; ts /= 24;
    long long d  = ts;

    long long yr = 1970;
    while (1) {
        int leap = (yr%4==0 && (yr%100!=0 || yr%400==0));
        int diy  = leap ? 366 : 365;
        if (d < diy) break;
        d -= diy; yr++;
    }
    static const int dm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int leap = (yr%4==0 && (yr%100!=0 || yr%400==0));
    int mo = 0;
    while (1) {
        int days = dm[mo]; if (mo==1 && leap) days++;
        if (d < days) break;
        d -= days; mo++;
    }

    snprintf(t->clock_str, sizeof(t->clock_str),
             "%02lld:%02lld:%02lld", h, m, s);
    snprintf(t->date_str,  sizeof(t->date_str),
             "%02d/%02lld/%lld", mo+1, d+1, yr);
}

static void layout_buttons(Taskbar *t) {
    int start_x = TASKBAR_START_W + TASKBAR_BTN_MARGIN * 2;
    int avail_w = t->w - start_x - TASKBAR_CLOCK_W -
                  t->tray_count * TASKBAR_TRAY_ITEM_W - 16;
    int btn_w = t->btn_count > 0
                ? (avail_w / t->btn_count)
                : TASKBAR_BTN_W;
    if (btn_w > TASKBAR_BTN_W) btn_w = TASKBAR_BTN_W;
    if (btn_w < 48) btn_w = 48;

    for (int i = 0; i < t->btn_count; i++) {
        t->btns[i].x = start_x + i * (btn_w + TASKBAR_BTN_MARGIN);
        t->btns[i].w = btn_w;
    }
}

Taskbar *taskbar_new(Compositor *comp, WindowManager *wm,
                     int screen_w, int screen_h) {
    Taskbar *t = xmalloc(sizeof(Taskbar));
    if (!t) return NULL;

    t->comp = comp;
    t->wm   = wm;
    t->w    = screen_w;
    t->h    = TASKBAR_HEIGHT;
    t->y    = screen_h - TASKBAR_HEIGHT;
    t->x    = 0;

    t->hovered_btn   = -1;
    t->hovered_tray  = -1;
    t->needs_redraw  = 1;

    TrayItem net = { TRAY_NETWORK, "Network", 0x4FC3F7FF, 1, NULL, NULL };
    TrayItem vol = { TRAY_VOLUME,  "Volume",  0x81C784FF, 1, NULL, NULL };
    taskbar_add_tray(t, &net);
    taskbar_add_tray(t, &vol);

    update_clock(t);
    return t;
}

void taskbar_free(Taskbar *t) {
    if (!t) return;
    free(t);
}

static void draw_start_button(Taskbar *t, NvSurface *surface) {
    int bx = t->x + TASKBAR_BTN_MARGIN;
    int by = t->y + (t->h - TASKBAR_BTN_H) / 2;
    int bw = TASKBAR_START_W - TASKBAR_BTN_MARGIN * 2;

    NvRect r = { bx, by, bw, TASKBAR_BTN_H };
    uint32_t bg = t->start_hovered ? START_HOVER : START_BG;
    draw_fill_rect(surface, &r, bg);

    int rx = t->start_pressed ? bx + 1 : bx;
    int ry = t->start_pressed ? by + 1 : by;
    font_draw_string(surface, rx + 10, ry + 8, "nv0", 0xFFFFFFFF, 13);

    if (t->launcher_open) {
        NvRect line = { bx, t->y, bw, 2 };
        draw_fill_rect(surface, &line, HIGHLIGHT_LINE);
    }
}

static void draw_task_buttons(Taskbar *t, NvSurface *surface) {
    for (int i = 0; i < t->btn_count; i++) {
        TaskBtn *b = &t->btns[i];
        int bx = t->x + b->x;
        int by = t->y + (t->h - TASKBAR_BTN_H) / 2;

        uint32_t bg = b->focused   ? BTN_FOCUSED  :
                      i == t->hovered_btn ? BTN_HOVERED :
                      b->minimized ? BTN_MINIMIZED : BTN_NORMAL;

        NvRect r = { bx, by, b->w, TASKBAR_BTN_H };
        draw_fill_rect(surface, &r, bg);
        draw_rect(surface, &r, BORDER_COLOR);

        if (b->focused) {
            NvRect top = { bx, t->y, b->w, 2 };
            draw_fill_rect(surface, &top, HIGHLIGHT_LINE);
        }

        uint32_t fg = b->focused ? BTN_TEXT_FOCUSED : BTN_TEXT_NORMAL;
        if (b->minimized) fg = 0x888888FF;

        char truncated[32];
        int max_chars = (b->w - TASKBAR_BTN_PAD_X * 2) / 7;
        if (max_chars < 1) max_chars = 1;
        if (max_chars > 30) max_chars = 30;
        strncpy(truncated, b->title, max_chars);
        truncated[max_chars] = '\0';
        if ((int)strlen(b->title) > max_chars && max_chars > 3) {
            truncated[max_chars-1] = '.';
            truncated[max_chars-2] = '.';
            truncated[max_chars-3] = '.';
        }

        font_draw_string(surface,
                         bx + TASKBAR_BTN_PAD_X,
                         by + (TASKBAR_BTN_H - 13) / 2,
                         truncated, fg, 13);
    }
}

static void draw_tray(Taskbar *t, NvSurface *surface) {
    int tray_start = t->x + t->w - TASKBAR_CLOCK_W -
                     t->tray_count * TASKBAR_TRAY_ITEM_W - 8;

    for (int i = 0; i < t->tray_count; i++) {
        TrayItem *ti = &t->tray[i];
        int tx = tray_start + i * TASKBAR_TRAY_ITEM_W;
        int ty = t->y + (t->h - 16) / 2;

        uint32_t col = ti->active ? ti->icon_color : TRAY_INACTIVE;

        if (i == t->hovered_tray) {
            NvRect hi = { tx - 2, t->y + 2, TASKBAR_TRAY_ITEM_W, t->h - 4 };
            draw_fill_rect(surface, &hi, BTN_HOVERED);
        }

        switch (ti->type) {
            case TRAY_NETWORK: {
                for (int bar = 0; bar < 4; bar++) {
                    int bh = (bar + 1) * 4;
                    NvRect br = { tx + bar * 5, ty + 16 - bh, 4, bh };
                    draw_fill_rect(surface, &br,
                                   bar < (ti->active ? 4 : 1) ? col : TRAY_INACTIVE);
                }
                break;
            }
            case TRAY_VOLUME: {
                NvRect cone  = { tx + 2, ty + 4, 6, 8 };
                NvRect wave1 = { tx + 10, ty + 5, 2, 6 };
                NvRect wave2 = { tx + 14, ty + 2, 2, 12 };
                draw_fill_rect(surface, &cone,  col);
                draw_fill_rect(surface, &wave1, col);
                if (ti->active) draw_fill_rect(surface, &wave2, col);
                break;
            }
            case TRAY_BATTERY: {
                NvRect body = { tx + 1, ty + 4, 16, 10 };
                NvRect tip  = { tx + 17, ty + 6, 3, 6 };
                draw_rect(surface, &body, col);
                draw_fill_rect(surface, &tip, col);
                int fill_w = (int)(14 * 0.75f);
                NvRect charge = { tx + 2, ty + 5, fill_w, 8 };
                draw_fill_rect(surface, &charge, col);
                break;
            }
            default: {
                NvRect dot = { tx + 7, ty + 5, 6, 6 };
                draw_fill_rect(surface, &dot, col);
                break;
            }
        }
    }
}

static void draw_clock(Taskbar *t, NvSurface *surface) {
    int cx = t->x + t->w - TASKBAR_CLOCK_W;
    int cy = t->y;

    NvRect r = { cx, cy, TASKBAR_CLOCK_W, t->h };
    draw_fill_rect(surface, &r, BG_COLOR);

    int tw = (int)strlen(t->clock_str) * 7;
    font_draw_string(surface,
                     cx + (TASKBAR_CLOCK_W - tw) / 2,
                     cy + 5,
                     t->clock_str, CLOCK_COLOR, 13);

    int dw = (int)strlen(t->date_str) * 6;
    font_draw_string(surface,
                     cx + (TASKBAR_CLOCK_W - dw) / 2,
                     cy + 20,
                     t->date_str, 0x888888FF, 11);
}

void taskbar_draw(Taskbar *t, NvSurface *surface) {
    if (!t || !surface) return;

    NvRect bg = { t->x, t->y, t->w, t->h };
    draw_fill_rect(surface, &bg, BG_COLOR);

    NvRect top_border = { t->x, t->y, t->w, 1 };
    draw_fill_rect(surface, &top_border, BORDER_COLOR);

    for (int px = t->x; px < t->x + t->w; px++) {
        uint32_t c = (px % 3 == 0) ? 0x2A2A2CFF : BG_COLOR;
        NvRect dot = { px, t->y + 1, 1, 1 };
        draw_fill_rect(surface, &dot, c);
    }

    draw_start_button(t, surface);
    draw_task_buttons(t, surface);
    draw_tray(t, surface);
    draw_clock(t, surface);

    int sep_x = t->x + TASKBAR_START_W;
    NvRect sep = { sep_x, t->y + 6, 1, t->h - 12 };
    draw_fill_rect(surface, &sep, SEPARATOR_COLOR);

    t->needs_redraw = 0;
}

void taskbar_mouse_down(Taskbar *t, int x, int y, int button) {
    if (!t) return;
    if (y < t->y || y >= t->y + t->h) return;

    int bx = t->x + TASKBAR_BTN_MARGIN;
    int bw = TASKBAR_START_W - TASKBAR_BTN_MARGIN * 2;

    if (x >= bx && x < bx + bw) {
        t->start_pressed = 1;
        t->launcher_open = !t->launcher_open;
        t->needs_redraw  = 1;
        if (t->comp) {
            launcher_toggle(t->comp, t->wm, t->x, t->y - 1);
        }
        return;
    }

    for (int i = 0; i < t->btn_count; i++) {
        int tbx = t->x + t->btns[i].x;
        if (x >= tbx && x < tbx + t->btns[i].w) {
            if (button == NV_MOUSE_LEFT) {
                if (t->btns[i].focused && !t->btns[i].minimized) {
                    wm_minimize_window(t->wm, t->btns[i].window_id);
                    t->btns[i].minimized = 1;
                    t->btns[i].focused   = 0;
                } else {
                    wm_focus_window(t->wm, t->btns[i].window_id);
                    if (t->btns[i].minimized) {
                        wm_restore_window(t->wm, t->btns[i].window_id);
                        t->btns[i].minimized = 0;
                    }
                    for (int j = 0; j < t->btn_count; j++)
                        t->btns[j].focused = (j == i);
                }
                t->needs_redraw = 1;
            } else if (button == NV_MOUSE_MIDDLE) {
                wm_close_window(t->wm, t->btns[i].window_id);
            }
            return;
        }
    }

    int tray_start = t->x + t->w - TASKBAR_CLOCK_W -
                     t->tray_count * TASKBAR_TRAY_ITEM_W - 8;
    for (int i = 0; i < t->tray_count; i++) {
        int tx = tray_start + i * TASKBAR_TRAY_ITEM_W;
        if (x >= tx && x < tx + TASKBAR_TRAY_ITEM_W) {
            if (t->tray[i].on_click)
                t->tray[i].on_click(t->tray[i].userdata);
            return;
        }
    }
}

void taskbar_mouse_move(Taskbar *t, int x, int y) {
    if (!t) return;

    int prev_start  = t->start_hovered;
    int prev_btn    = t->hovered_btn;
    int prev_tray   = t->hovered_tray;

    t->start_hovered = 0;
    t->hovered_btn   = -1;
    t->hovered_tray  = -1;

    if (y < t->y || y >= t->y + t->h) goto done;

    int sbx = t->x + TASKBAR_BTN_MARGIN;
    int sbw = TASKBAR_START_W - TASKBAR_BTN_MARGIN * 2;
    if (x >= sbx && x < sbx + sbw) { t->start_hovered = 1; goto done; }

    for (int i = 0; i < t->btn_count; i++) {
        int bx = t->x + t->btns[i].x;
        if (x >= bx && x < bx + t->btns[i].w) {
            t->hovered_btn = i;
            goto done;
        }
    }

    int tray_start = t->x + t->w - TASKBAR_CLOCK_W -
                     t->tray_count * TASKBAR_TRAY_ITEM_W - 8;
    for (int i = 0; i < t->tray_count; i++) {
        int tx = tray_start + i * TASKBAR_TRAY_ITEM_W;
        if (x >= tx && x < tx + TASKBAR_TRAY_ITEM_W) {
            t->hovered_tray = i;
            goto done;
        }
    }

done:
    if (t->start_hovered != prev_start ||
        t->hovered_btn   != prev_btn   ||
        t->hovered_tray  != prev_tray)
        t->needs_redraw = 1;
}

void taskbar_mouse_up(Taskbar *t, int x, int y) {
    if (!t) return;
    (void)x; (void)y;
    t->start_pressed = 0;
    t->needs_redraw  = 1;
}

void taskbar_add_window(Taskbar *t, int win_id, const char *title) {
    if (!t || t->btn_count >= TASKBAR_MAX_BTNS) return;
    TaskBtn *b = &t->btns[t->btn_count++];
    b->window_id = win_id;
    strncpy(b->title, title, sizeof(b->title) - 1);
    b->focused   = 1;
    b->minimized = 0;
    for (int i = 0; i < t->btn_count - 1; i++)
        t->btns[i].focused = 0;
    layout_buttons(t);
    t->needs_redraw = 1;
}

void taskbar_remove_window(Taskbar *t, int win_id) {
    if (!t) return;
    for (int i = 0; i < t->btn_count; i++) {
        if (t->btns[i].window_id != win_id) continue;
        memmove(&t->btns[i], &t->btns[i+1],
                sizeof(TaskBtn) * (t->btn_count - i - 1));
        t->btn_count--;
        layout_buttons(t);
        t->needs_redraw = 1;
        return;
    }
}

void taskbar_update_window(Taskbar *t, int win_id, const char *title,
                           int focused, int minimized) {
    if (!t) return;
    for (int i = 0; i < t->btn_count; i++) {
        if (t->btns[i].window_id != win_id) continue;
        if (title) strncpy(t->btns[i].title, title, sizeof(t->btns[i].title)-1);
        t->btns[i].focused   = focused;
        t->btns[i].minimized = minimized;
        if (focused) {
            for (int j = 0; j < t->btn_count; j++)
                if (j != i) t->btns[j].focused = 0;
        }
        t->needs_redraw = 1;
        return;
    }
}

void taskbar_add_tray(Taskbar *t, TrayItem *item) {
    if (!t || t->tray_count >= TASKBAR_MAX_TRAY) return;
    t->tray[t->tray_count++] = *item;
    t->needs_redraw = 1;
}

void taskbar_tick(Taskbar *t) {
    if (!t) return;
    t->tick_count++;
    if (t->tick_count % 60 == 0) {
        update_clock(t);
        t->needs_redraw = 1;
    }
}

int taskbar_height(void) { return TASKBAR_HEIGHT; }
int taskbar_y(Taskbar *t) { return t ? t->y : 0; }