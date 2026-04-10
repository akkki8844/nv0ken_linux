#include "slider.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TRACK_THICKNESS  4
#define THUMB_R          7
#define THUMB_D          (THUMB_R * 2)

static int val_to_pos(Slider *sl, int track_start, int track_len) {
    int range = sl->max_val - sl->min_val;
    if (range <= 0) return track_start;
    return track_start + ((sl->value - sl->min_val) * (track_len - THUMB_D)) / range;
}

static int pos_to_val(Slider *sl, int pos, int track_start, int track_len) {
    int range   = sl->max_val - sl->min_val;
    int usable  = track_len - THUMB_D;
    if (usable <= 0) return sl->min_val;
    int rel = pos - track_start;
    if (rel < 0)      rel = 0;
    if (rel > usable) rel = usable;
    int val = sl->min_val + (rel * range) / usable;
    if (sl->step > 1) {
        val = ((val - sl->min_val + sl->step/2) / sl->step) * sl->step + sl->min_val;
    }
    if (val < sl->min_val) val = sl->min_val;
    if (val > sl->max_val) val = sl->max_val;
    return val;
}

Slider *slider_new(int x, int y, int w, int h, SliderOrientation orient) {
    Slider *sl = malloc(sizeof(Slider));
    if (!sl) return NULL;
    memset(sl, 0, sizeof(Slider));
    widget_init(&sl->base, x, y, w, h);
    sl->orient    = orient;
    sl->min_val   = 0;
    sl->max_val   = 100;
    sl->value     = 0;
    sl->step      = 1;
    strncpy(sl->fmt, "%d", sizeof(sl->fmt)-1);
    sl->base.draw      = slider_draw;
    sl->base.on_event  = slider_on_event;
    return sl;
}

void slider_free(Slider *sl) {
    if (!sl) return;
    widget_free_base(&sl->base);
    free(sl);
}

void slider_set_range(Slider *sl, int min, int max) {
    if (!sl || min >= max) return;
    sl->min_val = min; sl->max_val = max;
    if (sl->value < min) sl->value = min;
    if (sl->value > max) sl->value = max;
    widget_set_dirty(&sl->base);
}

void slider_set_value(Slider *sl, int val) {
    if (!sl) return;
    if (val < sl->min_val) val = sl->min_val;
    if (val > sl->max_val) val = sl->max_val;
    sl->value = val;
    widget_set_dirty(&sl->base);
}

void slider_set_step(Slider *sl, int step) { if (sl && step > 0) sl->step = step; }

void slider_set_show_value(Slider *sl, int show, const char *fmt) {
    if (!sl) return;
    sl->show_value = show;
    if (fmt) strncpy(sl->fmt, fmt, sizeof(sl->fmt)-1);
}

void slider_set_on_change(Slider *sl, void (*cb)(void *, int), void *ud) {
    if (sl) { sl->on_change = cb; sl->change_ud = ud; }
}

int slider_value(const Slider *sl) { return sl ? sl->value : 0; }

void slider_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Slider *sl = (Slider *)w;
    int vert = (sl->orient == SLIDER_VERTICAL);
    int focused  = (w->flags & WIDGET_FOCUSED);
    int disabled = (w->flags & WIDGET_DISABLED);

    int track_x, track_y, track_len;
    if (vert) {
        track_x   = w->x + w->w / 2 - TRACK_THICKNESS / 2;
        track_y   = w->y + THUMB_R;
        track_len = w->h - THUMB_D;
    } else {
        track_x   = w->x + THUMB_R;
        track_y   = w->y + w->h / 2 - TRACK_THICKNESS / 2;
        track_len = w->w - THUMB_D;
    }

    /* track background */
    NvRect track_bg, track_fill;
    int thumb_pos = val_to_pos(sl, vert ? track_y : track_x, track_len);

    if (vert) {
        track_bg   = (NvRect){ track_x, track_y, TRACK_THICKNESS, track_len };
        track_fill = (NvRect){ track_x, thumb_pos, TRACK_THICKNESS,
                               track_y + track_len - thumb_pos };
    } else {
        track_bg   = (NvRect){ track_x, track_y, track_len, TRACK_THICKNESS };
        track_fill = (NvRect){ track_x, track_y, thumb_pos - track_x, TRACK_THICKNESS };
    }

    draw_fill_rounded_rect(dst, &track_bg, 0x333333FF, 2);
    if (!disabled) draw_fill_rounded_rect(dst, &track_fill, WT_ACCENT, 2);

    /* thumb */
    int tx = vert ? (w->x + w->w/2) : thumb_pos + THUMB_R;
    int ty = vert ? thumb_pos + THUMB_R : (w->y + w->h/2);

    uint32_t thumb_col = disabled   ? WT_FG_DISABLED :
                         sl->dragging ? WT_ACCENT_PRESS :
                         sl->hovered  ? WT_ACCENT_HOVER :
                                        0xFFFFFFFF;

    draw_fill_circle(dst, tx, ty, THUMB_R, thumb_col);
    draw_circle(dst, tx, ty, THUMB_R,
                focused ? WT_BORDER_FOCUS : 0x888888FF);
    draw_fill_circle(dst, tx, ty, THUMB_R / 3, WT_ACCENT);

    /* value label */
    if (sl->show_value) {
        char buf[32];
        snprintf(buf, sizeof(buf), sl->fmt, sl->value);
        int tw, th;
        font_measure_string(buf, 11, &tw, &th);
        int lx = vert ? w->x + w->w/2 - tw/2 : tx - tw/2;
        int ly = vert ? ty - THUMB_R - th - 2 : w->y;
        font_draw_string(dst, lx, ly, buf, WT_FG_DIM, 11);
    }

    w->flags &= ~WIDGET_DIRTY;
}

void slider_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Slider *sl = (Slider *)w;
    (void)ud;
    if (w->flags & WIDGET_DISABLED) return;

    int vert = (sl->orient == SLIDER_VERTICAL);
    int track_start = vert ? w->y + THUMB_R : w->x + THUMB_R;
    int track_len   = vert ? w->h - THUMB_D : w->w - THUMB_D;

    switch (ev->type) {
        case WIDGET_EV_MOUSE_ENTER: sl->hovered = 1; widget_set_dirty(w); break;
        case WIDGET_EV_MOUSE_LEAVE: sl->hovered = 0; sl->dragging = 0; widget_set_dirty(w); break;
        case WIDGET_EV_MOUSE_DOWN:
            sl->dragging = 1; {
            int pos = (vert ? ev->mouse.y : ev->mouse.x) - THUMB_R;
            int val = pos_to_val(sl, pos, track_start - THUMB_R, track_len);
            slider_set_value(sl, val);
            if (sl->on_change) sl->on_change(sl->change_ud, sl->value);
            }
            break;
        case WIDGET_EV_MOUSE_UP:
            sl->dragging = 0;
            widget_set_dirty(w);
            break;
        case WIDGET_EV_MOUSE_MOVE:
            if (sl->dragging) {
                int pos = (vert ? ev->mouse.y : ev->mouse.x) - THUMB_R;
                int val = pos_to_val(sl, pos, track_start - THUMB_R, track_len);
                slider_set_value(sl, val);
                if (sl->on_change) sl->on_change(sl->change_ud, sl->value);
            }
            break;
        case WIDGET_EV_KEY_DOWN:
            switch (ev->key.key) {
                case 0xFF52: case 0xFF53:
                    slider_set_value(sl, sl->value + sl->step);
                    if (sl->on_change) sl->on_change(sl->change_ud, sl->value);
                    break;
                case 0xFF51: case 0xFF54:
                    slider_set_value(sl, sl->value - sl->step);
                    if (sl->on_change) sl->on_change(sl->change_ud, sl->value);
                    break;
            }
            break;
        case WIDGET_EV_SCROLL:
            slider_set_value(sl, sl->value + ev->scroll.dy * sl->step);
            if (sl->on_change) sl->on_change(sl->change_ud, sl->value);
            break;
        case WIDGET_EV_FOCUS_IN:
        case WIDGET_EV_FOCUS_OUT:
            widget_set_dirty(w);
            break;
        default: break;
    }
}