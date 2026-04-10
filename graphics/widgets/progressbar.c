#include "progressbar.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Progressbar *progressbar_new(int x, int y, int w, int h) {
    Progressbar *pb = malloc(sizeof(Progressbar));
    if (!pb) return NULL;
    memset(pb, 0, sizeof(Progressbar));
    widget_init(&pb->base, x, y, w, h);
    pb->max_value      = 100;
    pb->show_text      = 1;
    pb->base.draw      = progressbar_draw;
    pb->base.on_event  = progressbar_on_event;
    return pb;
}

void progressbar_free(Progressbar *pb) {
    if (!pb) return;
    widget_free_base(&pb->base);
    free(pb);
}

void progressbar_set_value(Progressbar *pb, int v) {
    if (!pb) return;
    if (v < 0) v = 0;
    if (v > pb->max_value) v = pb->max_value;
    pb->value = v;
    widget_set_dirty(&pb->base);
}

void progressbar_set_max(Progressbar *pb, int max) {
    if (pb && max > 0) { pb->max_value = max; widget_set_dirty(&pb->base); }
}

void progressbar_set_style(Progressbar *pb, ProgressStyle s) {
    if (pb) { pb->style = s; widget_set_dirty(&pb->base); }
}

void progressbar_set_show_text(Progressbar *pb, int show) {
    if (pb) pb->show_text = show;
}

void progressbar_set_label(Progressbar *pb, const char *label) {
    if (pb && label) { strncpy(pb->label, label, sizeof(pb->label)-1); widget_set_dirty(&pb->base); }
}

int progressbar_value(const Progressbar *pb) { return pb ? pb->value : 0; }

void progressbar_tick(Progressbar *pb) {
    if (!pb || pb->style != PROGRESS_INDETERMINATE) return;
    pb->anim_timer++;
    if (pb->anim_timer % 2 == 0) {
        pb->anim_offset = (pb->anim_offset + 4) % (pb->base.w + 60);
        widget_set_dirty(&pb->base);
    }
}

void progressbar_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Progressbar *pb = (Progressbar *)w;

    NvRect bg = { w->x, w->y, w->w, w->h };
    draw_fill_rounded_rect(dst, &bg, 0x252526FF, 3);
    draw_rounded_rect(dst, &bg, WT_BORDER, 3);

    uint32_t fill_color =
        pb->style == PROGRESS_SUCCESS ? 0x4CAF50FF :
        pb->style == PROGRESS_ERROR   ? 0xF44336FF :
                                        WT_ACCENT;

    if (pb->style == PROGRESS_INDETERMINATE) {
        int seg_w = w->w / 3;
        int ox    = pb->anim_offset - 30;
        if (ox < 0) ox = 0;
        if (ox + seg_w > w->w) seg_w = w->w - ox;
        if (seg_w > 0) {
            NvRect fill = { w->x + 2 + ox, w->y + 2, seg_w, w->h - 4 };
            draw_fill_rounded_rect(dst, &fill, fill_color, 2);
        }
    } else {
        int fill_w = pb->max_value > 0
                     ? (pb->value * (w->w - 4)) / pb->max_value
                     : 0;
        if (fill_w > 0) {
            NvRect fill = { w->x + 2, w->y + 2, fill_w, w->h - 4 };
            draw_fill_rounded_rect(dst, &fill, fill_color, 2);

            NvRect shine = { w->x + 2, w->y + 2, fill_w, (w->h - 4) / 3 };
            draw_fill_rect_alpha(dst, &shine, 0xFFFFFF18);
        }
    }

    if (pb->show_text) {
        char buf[32];
        if (pb->label[0]) {
            strncpy(buf, pb->label, sizeof(buf)-1);
        } else if (pb->style != PROGRESS_INDETERMINATE) {
            int pct = pb->max_value > 0
                      ? (pb->value * 100) / pb->max_value : 0;
            snprintf(buf, sizeof(buf), "%d%%", pct);
        } else {
            buf[0] = '\0';
        }
        if (buf[0]) {
            int tw, th;
            font_measure_string(buf, 12, &tw, &th);
            font_draw_string(dst,
                             w->x + (w->w - tw) / 2,
                             w->y + (w->h - th) / 2,
                             buf, 0xFFFFFFFF, 12);
        }
    }

    w->flags &= ~WIDGET_DIRTY;
}

void progressbar_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    (void)w; (void)ev; (void)ud;
}