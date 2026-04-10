#include "scrollbar.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

#define ARROW_SIZE 14
#define MIN_THUMB  20

static void compute_thumb(Scrollbar *sb) {
    Widget *w = &sb->base;
    int track_len = (sb->orient == SCROLLBAR_VERTICAL)
                    ? w->h - ARROW_SIZE * 2
                    : w->w - ARROW_SIZE * 2;
    if (track_len <= 0) track_len = 1;
    int range = sb->max_val - sb->min_val;
    if (range <= 0) { sb->thumb_pos = 0; sb->thumb_size = track_len; return; }
    sb->thumb_size = (sb->page_size * track_len) / (range + sb->page_size);
    if (sb->thumb_size < MIN_THUMB) sb->thumb_size = MIN_THUMB;
    if (sb->thumb_size > track_len) sb->thumb_size = track_len;
    int scroll_range = range - sb->page_size;
    if (scroll_range <= 0) { sb->thumb_pos = 0; return; }
    sb->thumb_pos = ((sb->value - sb->min_val) * (track_len - sb->thumb_size))
                    / scroll_range;
}

static void set_value_from_thumb(Scrollbar *sb) {
    Widget *w = &sb->base;
    int track_len = (sb->orient == SCROLLBAR_VERTICAL)
                    ? w->h - ARROW_SIZE * 2
                    : w->w - ARROW_SIZE * 2;
    int scroll_range = (track_len - sb->thumb_size);
    if (scroll_range <= 0) { sb->value = sb->min_val; return; }
    int range = sb->max_val - sb->min_val - sb->page_size;
    if (range <= 0) { sb->value = sb->min_val; return; }
    sb->value = sb->min_val + (sb->thumb_pos * range) / scroll_range;
    if (sb->value < sb->min_val) sb->value = sb->min_val;
    if (sb->value > sb->max_val - sb->page_size)
        sb->value = sb->max_val - sb->page_size;
}

Scrollbar *scrollbar_new(int x, int y, int w, int h,
                          ScrollbarOrientation orient) {
    Scrollbar *sb = malloc(sizeof(Scrollbar));
    if (!sb) return NULL;
    memset(sb, 0, sizeof(Scrollbar));
    widget_init(&sb->base, x, y, w, h);
    sb->orient    = orient;
    sb->min_val   = 0;
    sb->max_val   = 100;
    sb->page_size = 10;
    sb->value     = 0;
    sb->base.draw      = scrollbar_draw;
    sb->base.on_event  = scrollbar_on_event;
    compute_thumb(sb);
    return sb;
}

void scrollbar_free(Scrollbar *sb) {
    if (!sb) return;
    widget_free_base(&sb->base);
    free(sb);
}

void scrollbar_set_range(Scrollbar *sb, int min, int max, int page) {
    if (!sb) return;
    sb->min_val   = min;
    sb->max_val   = max;
    sb->page_size = page;
    if (sb->value < min) sb->value = min;
    if (sb->value > max) sb->value = max;
    compute_thumb(sb);
    widget_set_dirty(&sb->base);
}

void scrollbar_set_value(Scrollbar *sb, int value) {
    if (!sb) return;
    if (value < sb->min_val) value = sb->min_val;
    if (value > sb->max_val - sb->page_size) value = sb->max_val - sb->page_size;
    sb->value = value;
    compute_thumb(sb);
    widget_set_dirty(&sb->base);
}

int scrollbar_value(const Scrollbar *sb) { return sb ? sb->value : 0; }

void scrollbar_set_on_scroll(Scrollbar *sb, void (*cb)(void *, int), void *ud) {
    if (sb) { sb->on_scroll = cb; sb->scroll_ud = ud; }
}

void scrollbar_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Scrollbar *sb = (Scrollbar *)w;
    int vert = (sb->orient == SCROLLBAR_VERTICAL);

    /* track */
    NvRect track = { w->x, w->y, w->w, w->h };
    draw_fill_rect(dst, &track, WT_SCROLLBAR_BG);

    /* arrows */
    NvRect arr1 = vert ? (NvRect){ w->x, w->y, w->w, ARROW_SIZE }
                       : (NvRect){ w->x, w->y, ARROW_SIZE, w->h };
    NvRect arr2 = vert ? (NvRect){ w->x, w->y + w->h - ARROW_SIZE, w->w, ARROW_SIZE }
                       : (NvRect){ w->x + w->w - ARROW_SIZE, w->y, ARROW_SIZE, w->h };

    draw_fill_rect(dst, &arr1, WT_BG_HOVER);
    draw_fill_rect(dst, &arr2, WT_BG_HOVER);
    draw_rect(dst, &arr1, WT_BORDER);
    draw_rect(dst, &arr2, WT_BORDER);

    /* arrow glyphs */
    int ax1 = arr1.x + arr1.w/2 - 2;
    int ay1 = arr1.y + arr1.h/2 - 2;
    int ax2 = arr2.x + arr2.w/2 - 2;
    int ay2 = arr2.y + arr2.h/2 - 2;
    if (vert) {
        draw_fill_triangle(dst, ax1, ay1+4, ax1+4, ay1, ax1+8, ay1+4, WT_FG_DIM);
        draw_fill_triangle(dst, ax2, ay2, ax2+4, ay2+4, ax2+8, ay2, WT_FG_DIM);
    } else {
        draw_fill_triangle(dst, ax1+4, ay1, ax1, ay1+4, ax1+4, ay1+8, WT_FG_DIM);
        draw_fill_triangle(dst, ax2, ay2, ax2+4, ay2+4, ax2+4, ay2, WT_FG_DIM);
    }

    /* thumb */
    NvRect thumb;
    if (vert)
        thumb = (NvRect){ w->x + 2, w->y + ARROW_SIZE + sb->thumb_pos,
                          w->w - 4, sb->thumb_size };
    else
        thumb = (NvRect){ w->x + ARROW_SIZE + sb->thumb_pos, w->y + 2,
                          sb->thumb_size, w->h - 4 };

    uint32_t thumb_col = sb->dragging      ? WT_ACCENT :
                         sb->hovered_thumb ? WT_SCROLLBAR_HOVER :
                                             WT_SCROLLBAR_FG;
    draw_fill_rounded_rect(dst, &thumb, thumb_col, 3);

    w->flags &= ~WIDGET_DIRTY;
}

void scrollbar_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Scrollbar *sb = (Scrollbar *)w;
    (void)ud;
    int vert = (sb->orient == SCROLLBAR_VERTICAL);

    switch (ev->type) {
        case WIDGET_EV_MOUSE_DOWN: {
            int pos = vert ? ev->mouse.y - w->y : ev->mouse.x - w->x;
            if (pos < ARROW_SIZE) {
                scrollbar_set_value(sb, sb->value - sb->page_size / 4);
                if (sb->on_scroll) sb->on_scroll(sb->scroll_ud, sb->value);
                break;
            }
            int track_end = (vert ? w->h : w->w) - ARROW_SIZE;
            if (pos >= track_end) {
                scrollbar_set_value(sb, sb->value + sb->page_size / 4);
                if (sb->on_scroll) sb->on_scroll(sb->scroll_ud, sb->value);
                break;
            }
            int thumb_start = ARROW_SIZE + sb->thumb_pos;
            int thumb_end   = thumb_start + sb->thumb_size;
            if (pos >= thumb_start && pos < thumb_end) {
                sb->dragging    = 1;
                sb->drag_offset = pos - thumb_start;
            } else {
                /* page jump */
                int dir = pos < thumb_start ? -1 : 1;
                scrollbar_set_value(sb, sb->value + dir * sb->page_size);
                if (sb->on_scroll) sb->on_scroll(sb->scroll_ud, sb->value);
            }
            widget_set_dirty(w);
            break;
        }
        case WIDGET_EV_MOUSE_UP:
            sb->dragging = 0;
            widget_set_dirty(w);
            break;
        case WIDGET_EV_MOUSE_MOVE:
            if (sb->dragging) {
                int pos = (vert ? ev->mouse.y - w->y : ev->mouse.x - w->x)
                          - ARROW_SIZE - sb->drag_offset;
                int track = (vert ? w->h : w->w) - ARROW_SIZE * 2 - sb->thumb_size;
                if (track < 1) track = 1;
                if (pos < 0) pos = 0;
                if (pos > track) pos = track;
                sb->thumb_pos = pos;
                set_value_from_thumb(sb);
                if (sb->on_scroll) sb->on_scroll(sb->scroll_ud, sb->value);
                widget_set_dirty(w);
            }
            break;
        case WIDGET_EV_SCROLL:
            scrollbar_set_value(sb, sb->value + ev->scroll.dy * 3);
            if (sb->on_scroll) sb->on_scroll(sb->scroll_ud, sb->value);
            widget_set_dirty(w);
            break;
        default: break;
    }
}