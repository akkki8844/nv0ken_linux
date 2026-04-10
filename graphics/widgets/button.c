#include "button.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>

Button *button_new(int x, int y, int w, int h, const char *label,
                   ButtonStyle style) {
    Button *btn = malloc(sizeof(Button));
    if (!btn) return NULL;
    memset(btn, 0, sizeof(Button));
    widget_init(&btn->base, x, y, w, h);
    if (label) strncpy(btn->label, label, sizeof(btn->label) - 1);
    btn->style           = style;
    btn->base.draw       = button_draw;
    btn->base.on_event   = button_on_event;
    return btn;
}

void button_free(Button *btn) {
    if (!btn) return;
    widget_free_base(&btn->base);
    free(btn);
}

void button_set_label(Button *btn, const char *label) {
    if (!btn || !label) return;
    strncpy(btn->label, label, sizeof(btn->label) - 1);
    widget_set_dirty(&btn->base);
}

void button_set_style(Button *btn, ButtonStyle style) {
    if (!btn) return;
    btn->style = style;
    widget_set_dirty(&btn->base);
}

void button_set_toggle(Button *btn, int enabled, int initial) {
    if (!btn) return;
    btn->toggle_mode = enabled;
    btn->toggled     = initial;
    widget_set_dirty(&btn->base);
}

void button_set_on_click(Button *btn, void (*cb)(void *), void *ud) {
    if (!btn) return;
    btn->on_click      = cb;
    btn->click_userdata = ud;
}

int button_is_toggled(const Button *btn) {
    return btn ? btn->toggled : 0;
}

void button_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Button *btn = (Button *)w;
    int disabled = (w->flags & WIDGET_DISABLED);
    int focused  = (w->flags & WIDGET_FOCUSED);

    uint32_t bg, fg, border;

    if (disabled) {
        bg = WT_BG_DISABLED; fg = WT_FG_DISABLED; border = WT_BORDER;
    } else if (btn->pressed || (btn->toggle_mode && btn->toggled)) {
        switch (btn->style) {
            case BTN_STYLE_PRIMARY: bg = WT_ACCENT_PRESS; break;
            case BTN_STYLE_DANGER:  bg = 0xA80000FF;      break;
            case BTN_STYLE_FLAT:    bg = WT_BG_HOVER;     break;
            default:                bg = WT_BG_PRESS;     break;
        }
        fg = 0xFFFFFFFF; border = WT_ACCENT_PRESS;
    } else if (btn->hovered) {
        switch (btn->style) {
            case BTN_STYLE_PRIMARY: bg = WT_ACCENT_HOVER; break;
            case BTN_STYLE_DANGER:  bg = 0xC00000FF;      break;
            case BTN_STYLE_FLAT:    bg = WT_BG_HOVER;     break;
            default:                bg = WT_BG_HOVER;     break;
        }
        fg = WT_FG; border = WT_BORDER_HOVER;
    } else {
        switch (btn->style) {
            case BTN_STYLE_PRIMARY: bg = WT_ACCENT;  border = WT_ACCENT;    break;
            case BTN_STYLE_DANGER:  bg = 0xB00000FF; border = 0xC00000FF;   break;
            case BTN_STYLE_FLAT:    bg = 0x00000000; border = 0x00000000;   break;
            default:                bg = WT_BG;      border = WT_BORDER;    break;
        }
        fg = WT_FG;
    }

    NvRect r = { w->x, w->y, w->w, w->h };
    draw_fill_rounded_rect(dst, &r, bg, 4);
    if (border && btn->style != BTN_STYLE_FLAT)
        draw_rounded_rect(dst, &r, focused ? WT_BORDER_FOCUS : border, 4);

    if (btn->label[0]) {
        int tw, th;
        font_measure_string(btn->label, 13, &tw, &th);
        int tx = w->x + (w->w - tw) / 2;
        int ty = w->y + (w->h - 13) / 2;
        font_draw_string(dst, tx, ty, btn->label, fg, 13);
    }

    w->flags &= ~WIDGET_DIRTY;
}

void button_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Button *btn = (Button *)w;
    (void)ud;

    switch (ev->type) {
        case WIDGET_EV_MOUSE_ENTER:
            btn->hovered = 1;
            widget_set_dirty(w);
            break;
        case WIDGET_EV_MOUSE_LEAVE:
            btn->hovered = 0;
            btn->pressed = 0;
            widget_set_dirty(w);
            break;
        case WIDGET_EV_MOUSE_DOWN:
            if (ev->mouse.button == 1 && !(w->flags & WIDGET_DISABLED)) {
                btn->pressed = 1;
                widget_set_dirty(w);
            }
            break;
        case WIDGET_EV_MOUSE_UP:
            if (btn->pressed && !(w->flags & WIDGET_DISABLED)) {
                btn->pressed = 0;
                if (btn->toggle_mode) {
                    btn->toggled = !btn->toggled;
                }
                if (btn->on_click) btn->on_click(btn->click_userdata);
                WidgetEvent change = { .type = WIDGET_EV_ACTIVATE };
                widget_dispatch(w, &change);
            }
            btn->pressed = 0;
            widget_set_dirty(w);
            break;
        case WIDGET_EV_KEY_DOWN:
            if ((ev->key.key == ' ' || ev->key.key == '\r') &&
                !(w->flags & WIDGET_DISABLED)) {
                if (btn->toggle_mode) btn->toggled = !btn->toggled;
                if (btn->on_click) btn->on_click(btn->click_userdata);
                WidgetEvent act = { .type = WIDGET_EV_ACTIVATE };
                widget_dispatch(w, &act);
                widget_set_dirty(w);
            }
            break;
        case WIDGET_EV_FOCUS_IN:
        case WIDGET_EV_FOCUS_OUT:
            widget_set_dirty(w);
            break;
        default:
            break;
    }
}