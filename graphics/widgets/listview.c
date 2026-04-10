#include "listview.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>

#define HEADER_H    22
#define SB_W        12
#define PAD         6

static void update_scrollbar(ListView *lv) {
    if (!lv->scrollbar) return;
    int visible = lv->base.h / lv->item_h;
    int header_offset = lv->show_header ? HEADER_H : 0;
    visible = (lv->base.h - header_offset) / lv->item_h;
    scrollbar_set_range(lv->scrollbar, 0, lv->item_count, visible);
}

static void on_scroll_cb(void *ud, int value) {
    ListView *lv = (ListView *)ud;
    lv->scroll_offset = value;
    widget_set_dirty(&lv->base);
}

ListView *listview_new(int x, int y, int w, int h) {
    ListView *lv = malloc(sizeof(ListView));
    if (!lv) return NULL;
    memset(lv, 0, sizeof(ListView));
    widget_init(&lv->base, x, y, w, h);
    lv->item_h       = LISTVIEW_ITEM_H;
    lv->selected     = -1;
    lv->hovered      = -1;
    lv->base.draw    = listview_draw;
    lv->base.on_event= listview_on_event;

    lv->scrollbar = scrollbar_new(x + w - SB_W, y, SB_W, h,
                                   SCROLLBAR_VERTICAL);
    scrollbar_set_on_scroll(lv->scrollbar, on_scroll_cb, lv);
    widget_add_child(&lv->base, &lv->scrollbar->base);
    return lv;
}

void listview_free(ListView *lv) {
    if (!lv) return;
    scrollbar_free(lv->scrollbar);
    widget_free_base(&lv->base);
    free(lv);
}

void listview_clear(ListView *lv) {
    if (!lv) return;
    lv->item_count  = 0;
    lv->selected    = -1;
    lv->hovered     = -1;
    lv->scroll_offset = 0;
    update_scrollbar(lv);
    widget_set_dirty(&lv->base);
}

int listview_add_item(ListView *lv, const char *text) {
    if (!lv || lv->item_count >= LISTVIEW_MAX_ITEMS) return -1;
    ListItem *item = &lv->items[lv->item_count];
    memset(item, 0, sizeof(ListItem));
    if (text) strncpy(item->text, text, sizeof(item->text) - 1);
    item->color    = WT_FG;
    item->bg_color = 0x00000000;
    int idx = lv->item_count++;
    update_scrollbar(lv);
    widget_set_dirty(&lv->base);
    return idx;
}

void listview_set_item_text(ListView *lv, int idx, const char *text) {
    if (!lv || idx < 0 || idx >= lv->item_count || !text) return;
    strncpy(lv->items[idx].text, text, sizeof(lv->items[idx].text) - 1);
    widget_set_dirty(&lv->base);
}

void listview_set_item_col(ListView *lv, int idx, int col, const char *text) {
    if (!lv || idx < 0 || idx >= lv->item_count) return;
    if (col < 0 || col >= LISTVIEW_MAX_COLS) return;
    if (text) strncpy(lv->items[idx].cols[col], text, 63);
    if (col >= lv->items[idx].col_count) lv->items[idx].col_count = col + 1;
    widget_set_dirty(&lv->base);
}

void listview_set_item_color(ListView *lv, int idx, uint32_t fg, uint32_t bg) {
    if (!lv || idx < 0 || idx >= lv->item_count) return;
    lv->items[idx].color    = fg;
    lv->items[idx].bg_color = bg;
    widget_set_dirty(&lv->base);
}

void listview_set_item_data(ListView *lv, int idx, void *data) {
    if (lv && idx >= 0 && idx < lv->item_count)
        lv->items[idx].data = data;
}

void listview_remove_item(ListView *lv, int idx) {
    if (!lv || idx < 0 || idx >= lv->item_count) return;
    memmove(&lv->items[idx], &lv->items[idx+1],
            sizeof(ListItem) * (lv->item_count - idx - 1));
    lv->item_count--;
    if (lv->selected >= lv->item_count) lv->selected = lv->item_count - 1;
    update_scrollbar(lv);
    widget_set_dirty(&lv->base);
}

void listview_set_selected(ListView *lv, int idx) {
    if (!lv) return;
    if (idx < -1 || idx >= lv->item_count) return;
    lv->selected = idx;
    listview_scroll_to(lv, idx);
    widget_set_dirty(&lv->base);
}

int listview_selected(const ListView *lv) { return lv ? lv->selected : -1; }

ListItem *listview_selected_item(ListView *lv) {
    if (!lv || lv->selected < 0) return NULL;
    return &lv->items[lv->selected];
}

void listview_set_columns(ListView *lv, int count,
                           const char **headers, const int *widths) {
    if (!lv) return;
    lv->col_count   = count > LISTVIEW_MAX_COLS ? LISTVIEW_MAX_COLS : count;
    lv->show_header = (count > 0);
    for (int i = 0; i < lv->col_count; i++) {
        if (headers) strncpy(lv->headers[i], headers[i], 63);
        if (widths)  lv->col_widths[i] = widths[i];
        else         lv->col_widths[i] = 80;
    }
    widget_set_dirty(&lv->base);
}

void listview_scroll_to(ListView *lv, int idx) {
    if (!lv || idx < 0) return;
    int header_h = lv->show_header ? HEADER_H : 0;
    int visible  = (lv->base.h - header_h) / lv->item_h;
    if (idx < lv->scroll_offset)
        lv->scroll_offset = idx;
    else if (idx >= lv->scroll_offset + visible)
        lv->scroll_offset = idx - visible + 1;
    scrollbar_set_value(lv->scrollbar, lv->scroll_offset);
    widget_set_dirty(&lv->base);
}

void listview_set_on_select(ListView *lv, void (*cb)(void *, int), void *ud) {
    if (lv) { lv->on_select = cb; lv->list_ud = ud; }
}
void listview_set_on_activate(ListView *lv, void (*cb)(void *, int), void *ud) {
    if (lv) { lv->on_activate = cb; lv->list_ud = ud; }
}

void listview_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    ListView *lv = (ListView *)w;

    NvRect bg = { w->x, w->y, w->w, w->h };
    draw_fill_rect(dst, &bg, WT_BG);
    draw_rect(dst, &bg, (w->flags & WIDGET_FOCUSED) ? WT_BORDER_FOCUS : WT_BORDER);

    int list_w  = w->w - SB_W - 2;
    int header_h = lv->show_header ? HEADER_H : 0;
    int item_y  = w->y + header_h;

    /* header */
    if (lv->show_header) {
        NvRect hr = { w->x, w->y, list_w, HEADER_H };
        draw_fill_rect(dst, &hr, 0x2D2D30FF);
        NvRect hbr = { w->x, w->y + HEADER_H - 1, list_w, 1 };
        draw_fill_rect(dst, &hbr, WT_BORDER);
        int hx = w->x + PAD;
        for (int c = 0; c < lv->col_count; c++) {
            int cw = lv->col_widths[c];
            font_draw_string_clip(dst, hx, w->y + 5,
                                  lv->headers[c], WT_FG_DIM, 12, cw - PAD);
            hx += cw;
            if (hx >= w->x + list_w) break;
        }
    }

    /* items */
    int visible = (w->h - header_h) / lv->item_h + 1;
    for (int i = 0; i < visible; i++) {
        int idx = i + lv->scroll_offset;
        if (idx >= lv->item_count) break;
        ListItem *item = &lv->items[idx];
        int iy = item_y + i * lv->item_h;
        if (iy + lv->item_h > w->y + w->h) break;

        uint32_t row_bg =
            (idx == lv->selected) ? WT_SELECTION :
            (idx == lv->hovered)  ? WT_BG_HOVER  :
            (idx % 2 == 0)        ? 0x00000000   :
                                    0x00000010;

        if (COLOR_A(item->bg_color)) row_bg = item->bg_color;

        if (row_bg) {
            NvRect rr = { w->x + 1, iy, list_w - 2, lv->item_h };
            draw_fill_rect(dst, &rr, row_bg);
        }

        if (idx == lv->selected) {
            NvRect accent = { w->x + 1, iy, 3, lv->item_h };
            draw_fill_rect(dst, &accent, WT_ACCENT);
        }

        uint32_t fg = item->color ? item->color : WT_FG;
        int tx = w->x + PAD;

        if (lv->col_count > 0) {
            for (int c = 0; c < lv->col_count; c++) {
                int cw = lv->col_widths[c];
                const char *ct = (c < item->col_count) ? item->cols[c] : item->text;
                if (c == 0 && !item->col_count) ct = item->text;
                font_draw_string_clip(dst, tx,
                                      iy + (lv->item_h - 13) / 2,
                                      ct, fg, 13, cw - PAD);
                tx += cw;
                if (tx >= w->x + list_w) break;
            }
        } else {
            font_draw_string_clip(dst, tx,
                                  iy + (lv->item_h - 13) / 2,
                                  item->text, fg, 13,
                                  list_w - PAD * 2);
        }
    }

    w->flags &= ~WIDGET_DIRTY;
}

void listview_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    ListView *lv = (ListView *)w;
    (void)ud;
    int header_h = lv->show_header ? HEADER_H : 0;

    switch (ev->type) {
        case WIDGET_EV_MOUSE_MOVE: {
            int rel_y = ev->mouse.y - w->y - header_h;
            int idx   = lv->scroll_offset + rel_y / lv->item_h;
            int prev  = lv->hovered;
            lv->hovered = (idx >= 0 && idx < lv->item_count) ? idx : -1;
            if (lv->hovered != prev) widget_set_dirty(w);
            /* forward to scrollbar */
            WidgetEvent fwd = *ev;
            fwd.mouse.x = ev->mouse.x;
            fwd.mouse.y = ev->mouse.y;
            if (widget_hit(&lv->scrollbar->base, ev->mouse.x, ev->mouse.y))
                widget_dispatch(&lv->scrollbar->base, &fwd);
            break;
        }
        case WIDGET_EV_MOUSE_DOWN: {
            if (widget_hit(&lv->scrollbar->base, ev->mouse.x, ev->mouse.y)) {
                widget_dispatch(&lv->scrollbar->base, ev);
                break;
            }
            int rel_y = ev->mouse.y - w->y - header_h;
            if (rel_y < 0) break;
            int idx = lv->scroll_offset + rel_y / lv->item_h;
            if (idx >= 0 && idx < lv->item_count) {
                lv->selected = idx;
                if (lv->on_select) lv->on_select(lv->list_ud, idx);
                WidgetEvent sel = { .type = WIDGET_EV_CHANGE };
                widget_dispatch(w, &sel);
                widget_set_dirty(w);
            }
            break;
        }
        case WIDGET_EV_KEY_DOWN:
            switch (ev->key.key) {
                case 0xFF52: /* up */
                    if (lv->selected > 0) {
                        lv->selected--;
                        listview_scroll_to(lv, lv->selected);
                        if (lv->on_select) lv->on_select(lv->list_ud, lv->selected);
                        widget_set_dirty(w);
                    }
                    break;
                case 0xFF54: /* down */
                    if (lv->selected < lv->item_count - 1) {
                        lv->selected++;
                        listview_scroll_to(lv, lv->selected);
                        if (lv->on_select) lv->on_select(lv->list_ud, lv->selected);
                        widget_set_dirty(w);
                    }
                    break;
                case '\r':
                    if (lv->selected >= 0 && lv->on_activate)
                        lv->on_activate(lv->list_ud, lv->selected);
                    break;
            }
            break;
        case WIDGET_EV_SCROLL:
            lv->scroll_offset += ev->scroll.dy * 3;
            if (lv->scroll_offset < 0) lv->scroll_offset = 0;
            if (lv->scroll_offset > lv->item_count - 1)
                lv->scroll_offset = lv->item_count - 1;
            if (lv->scroll_offset < 0) lv->scroll_offset = 0;
            scrollbar_set_value(lv->scrollbar, lv->scroll_offset);
            widget_set_dirty(w);
            break;
        default: break;
    }
}