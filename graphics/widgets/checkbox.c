#include "checkbox.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>

#define BOX_SIZE   14
#define BOX_PAD     3

Checkbox *checkbox_new(int x, int y, int w, int h, const char *label) {
    Checkbox *cb = malloc(sizeof(Checkbox));
    if (!cb) return NULL;
    memset(cb, 0, sizeof(Checkbox));
    widget_init(&cb->base, x, y, w, h);
    if (label) strncpy(cb->label, label, sizeof(cb->label) - 1);
    cb->state          = CHECK_UNCHECKED;
    cb->base.draw      = checkbox_draw;
    cb->base.on_event  = checkbox_on_event;
    return cb;
}

void checkbox_free(Checkbox *cb) {
    if (!cb) return;
    widget_free_base(&cb->base);
    free(cb);
}

void checkbox_set_label(Checkbox *cb, const char *label) {
    if (!cb || !label) return;
    strncpy(cb->label, label, sizeof(cb->label) - 1);
    widget_set_dirty(&cb->base);
}

void checkbox_set_state(Checkbox *cb, CheckState state) {
    if (!cb) return;
    cb->state = state;
    widget_set_dirty(&cb->base);
}

CheckState checkbox_state(const Checkbox *cb) {
    return cb ? cb->state : CHECK_UNCHECKED;
}

int checkbox_checked(const Checkbox *cb) {
    return cb ? (cb->state == CHECK_CHECKED) : 0;
}

void checkbox_set_radio_mode(Checkbox *cb, int radio) {
    if (cb) cb->radio_mode = radio;
}

void checkbox_set_on_change(Checkbox *cb, void (*fn)(void *, CheckState), void *ud) {
    if (cb) { cb->on_change = fn; cb->change_ud = ud; }
}

void checkbox_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Checkbox *cb = (Checkbox *)w;
    int disabled = (w->flags & WIDGET_DISABLED);
    int focused  = (w->flags & WIDGET_FOCUSED);

    int bx = w->x + BOX_PAD;
    int by = w->y + (w->h - BOX_SIZE) / 2;

    /* box background */
    NvRect box = { bx, by, BOX_SIZE, BOX_SIZE };
    uint32_t box_bg =
        cb->state == CHECK_CHECKED  ? (disabled ? WT_FG_DISABLED : WT_ACCENT) :
        cb->pressed                 ? WT_BG_PRESS :
        cb->hovered                 ? WT_BG_HOVER :
                                      WT_INPUT_BG;
    uint32_t box_border =
        focused  ? WT_BORDER_FOCUS :
        cb->hovered ? WT_BORDER_HOVER :
                    WT_BORDER;

    if (cb->radio_mode) {
        draw_fill_circle(dst, bx + BOX_SIZE/2, by + BOX_SIZE/2,
                         BOX_SIZE/2, box_bg);
        draw_circle(dst, bx + BOX_SIZE/2, by + BOX_SIZE/2,
                    BOX_SIZE/2, box_border);
        if (cb->state == CHECK_CHECKED)
            draw_fill_circle(dst, bx + BOX_SIZE/2, by + BOX_SIZE/2,
                             BOX_SIZE/4, 0xFFFFFFFF);
    } else {
        draw_fill_rounded_rect(dst, &box, box_bg, 2);
        draw_rounded_rect(dst, &box, box_border, 2);

        if (cb->state == CHECK_CHECKED) {
            /* checkmark */
            draw_line(dst, bx+2, by+7, bx+5, by+10, 0xFFFFFFFF);
            draw_line(dst, bx+5, by+10, bx+12, by+3, 0xFFFFFFFF);
            draw_line(dst, bx+3, by+7, bx+5, by+9, 0xFFFFFFFF);
            draw_line(dst, bx+5, by+9, bx+11, by+3, 0xFFFFFFFF);
        } else if (cb->state == CHECK_INDETERMINATE) {
            NvRect dash = { bx+3, by+6, BOX_SIZE-6, 2 };
            draw_fill_rect(dst, &dash, WT_ACCENT);
        }
    }

    if (cb->label[0]) {
        int tx = bx + BOX_SIZE + BOX_PAD + 2;
        int ty = w->y + (w->h - 13) / 2;
        uint32_t fg = disabled ? WT_FG_DISABLED : WT_FG;
        font_draw_string(dst, tx, ty, cb->label, fg, 13);
    }

    w->flags &= ~WIDGET_DIRTY;
}

void checkbox_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Checkbox *cb = (Checkbox *)w;
    (void)ud;

    switch (ev->type) {
        case WIDGET_EV_MOUSE_ENTER:  cb->hovered = 1; widget_set_dirty(w); break;
        case WIDGET_EV_MOUSE_LEAVE:  cb->hovered = 0; cb->pressed = 0; widget_set_dirty(w); break;
        case WIDGET_EV_MOUSE_DOWN:
            if (!(w->flags & WIDGET_DISABLED)) { cb->pressed = 1; widget_set_dirty(w); }
            break;
        case WIDGET_EV_MOUSE_UP:
            if (cb->pressed && !(w->flags & WIDGET_DISABLED)) {
                cb->pressed = 0;
                if (cb->radio_mode) {
                    cb->state = CHECK_CHECKED;
                } else {
                    cb->state = (cb->state == CHECK_CHECKED)
                                ? CHECK_UNCHECKED : CHECK_CHECKED;
                }
                if (cb->on_change) cb->on_change(cb->change_ud, cb->state);
                WidgetEvent ch = { .type = WIDGET_EV_CHANGE };
                widget_dispatch(w, &ch);
                widget_set_dirty(w);
            }
            break;
        case WIDGET_EV_KEY_DOWN:
            if ((ev->key.key == ' ') && !(w->flags & WIDGET_DISABLED)) {
                cb->state = (cb->state == CHECK_CHECKED)
                            ? CHECK_UNCHECKED : CHECK_CHECKED;
                if (cb->on_change) cb->on_change(cb->change_ud, cb->state);
                widget_set_dirty(w);
            }
            break;
        case WIDGET_EV_FOCUS_IN:
        case WIDGET_EV_FOCUS_OUT:
            widget_set_dirty(w);
            break;
        default: break;
    }
}