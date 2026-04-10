#include "textbox.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>

#define CELL_W  7
#define CELL_H  14
#define PAD_X   6
#define PAD_Y   4
#define CURSOR_W 2

static void push_undo(Textbox *tb) {
    if (tb->undo_count < TEXTBOX_HISTORY) {
        strncpy(tb->undo_stack[tb->undo_count], tb->text, TEXTBOX_MAX_LEN - 1);
        tb->undo_cursor[tb->undo_count] = tb->cursor;
        tb->undo_count++;
        tb->undo_pos = tb->undo_count;
    }
}

static void notify_change(Textbox *tb) {
    if (tb->on_change) tb->on_change(tb->callback_ud, tb->text);
    WidgetEvent ev = { .type = WIDGET_EV_CHANGE };
    widget_dispatch(&tb->base, &ev);
    widget_set_dirty(&tb->base);
}

static void insert_char(Textbox *tb, uint32_t cp) {
    if (tb->readonly) return;
    int max = tb->max_len > 0 ? tb->max_len : TEXTBOX_MAX_LEN - 1;
    if (tb->len >= max) return;

    if (tb->has_selection) {
        int a = tb->sel_start < tb->sel_end ? tb->sel_start : tb->sel_end;
        int b = tb->sel_start > tb->sel_end ? tb->sel_start : tb->sel_end;
        memmove(tb->text + a, tb->text + b, tb->len - b + 1);
        tb->len -= b - a;
        tb->cursor = a;
        tb->has_selection = 0;
    }

    push_undo(tb);
    memmove(tb->text + tb->cursor + 1,
            tb->text + tb->cursor,
            tb->len - tb->cursor + 1);
    tb->text[tb->cursor] = (char)cp;
    tb->len++;
    tb->cursor++;
    notify_change(tb);
}

static void delete_selection(Textbox *tb) {
    if (!tb->has_selection) return;
    int a = tb->sel_start < tb->sel_end ? tb->sel_start : tb->sel_end;
    int b = tb->sel_start > tb->sel_end ? tb->sel_start : tb->sel_end;
    push_undo(tb);
    memmove(tb->text + a, tb->text + b, tb->len - b + 1);
    tb->len -= b - a;
    tb->cursor = a;
    tb->has_selection = 0;
    notify_change(tb);
}

static void ensure_cursor_visible(Textbox *tb) {
    int visible_chars = (tb->base.w - PAD_X * 2) / CELL_W;
    if (visible_chars < 1) visible_chars = 1;
    if (tb->cursor < tb->scroll_x)
        tb->scroll_x = tb->cursor;
    if (tb->cursor > tb->scroll_x + visible_chars - 1)
        tb->scroll_x = tb->cursor - visible_chars + 1;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

Textbox *textbox_new(int x, int y, int w, int h) {
    Textbox *tb = malloc(sizeof(Textbox));
    if (!tb) return NULL;
    memset(tb, 0, sizeof(Textbox));
    widget_init(&tb->base, x, y, w, h);
    tb->max_len        = TEXTBOX_MAX_LEN - 1;
    tb->base.draw      = textbox_draw;
    tb->base.on_event  = textbox_on_event;
    return tb;
}

void textbox_free(Textbox *tb) {
    if (!tb) return;
    widget_free_base(&tb->base);
    free(tb);
}

void textbox_set_text(Textbox *tb, const char *text) {
    if (!tb) return;
    memset(tb->text, 0, sizeof(tb->text));
    if (text) strncpy(tb->text, text, sizeof(tb->text) - 1);
    tb->len           = (int)strlen(tb->text);
    tb->cursor        = tb->len;
    tb->has_selection = 0;
    tb->scroll_x      = 0;
    widget_set_dirty(&tb->base);
}

void textbox_set_placeholder(Textbox *tb, const char *ph) {
    if (tb && ph) strncpy(tb->placeholder, ph, sizeof(tb->placeholder) - 1);
}

void textbox_set_readonly(Textbox *tb, int ro) { if (tb) tb->readonly = ro; }
void textbox_set_password(Textbox *tb, int pw) { if (tb) { tb->password = pw; widget_set_dirty(&tb->base); } }
void textbox_set_max_len(Textbox *tb, int max) { if (tb) tb->max_len = max; }

void textbox_set_on_change(Textbox *tb, void (*cb)(void *, const char *), void *ud) {
    if (tb) { tb->on_change = cb; tb->callback_ud = ud; }
}
void textbox_set_on_enter(Textbox *tb, void (*cb)(void *, const char *), void *ud) {
    if (tb) { tb->on_enter = cb; tb->callback_ud = ud; }
}

const char *textbox_text(const Textbox *tb) { return tb ? tb->text : ""; }

void textbox_select_all(Textbox *tb) {
    if (!tb) return;
    tb->sel_start = 0; tb->sel_end = tb->len;
    tb->cursor    = tb->len;
    tb->has_selection = (tb->len > 0);
    widget_set_dirty(&tb->base);
}

void textbox_clear(Textbox *tb) {
    if (!tb) return;
    textbox_set_text(tb, "");
    notify_change(tb);
}

/* -----------------------------------------------------------------------
 * Draw
 * --------------------------------------------------------------------- */

void textbox_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Textbox *tb = (Textbox *)w;
    int focused  = (w->flags & WIDGET_FOCUSED);
    int disabled = (w->flags & WIDGET_DISABLED);

    NvRect r = { w->x, w->y, w->w, w->h };
    draw_fill_rect(dst, &r, disabled ? WT_BG_DISABLED : WT_INPUT_BG);
    draw_rect(dst, &r,
              focused  ? WT_BORDER_FOCUS :
              disabled ? WT_BORDER : WT_BORDER);

    if (focused) {
        NvRect bottom = { w->x + 1, w->y + w->h - 2, w->w - 2, 2 };
        draw_fill_rect(dst, &bottom, WT_ACCENT);
    }

    int text_x = w->x + PAD_X;
    int text_y = w->y + (w->h - CELL_H) / 2;

    if (!tb->len && tb->placeholder[0] && !focused) {
        font_draw_string_clip(dst, text_x, text_y,
                              tb->placeholder, WT_FG_DIM, 13,
                              w->w - PAD_X * 2);
        w->flags &= ~WIDGET_DIRTY;
        return;
    }

    /* Selection background */
    if (tb->has_selection && focused) {
        int a = (tb->sel_start < tb->sel_end ? tb->sel_start : tb->sel_end)
                - tb->scroll_x;
        int b = (tb->sel_start > tb->sel_end ? tb->sel_start : tb->sel_end)
                - tb->scroll_x;
        if (a < 0) a = 0;
        int sel_x = text_x + a * CELL_W;
        int sel_w = (b - a) * CELL_W;
        NvRect sel_r = { sel_x, text_y - 1, sel_w, CELL_H + 2 };
        draw_fill_rect(dst, &sel_r, WT_SELECTION);
    }

    /* Text */
    char display[TEXTBOX_MAX_LEN];
    if (tb->password) {
        memset(display, '*', tb->len);
        display[tb->len] = '\0';
    } else {
        strncpy(display, tb->text, sizeof(display) - 1);
    }

    const char *vis_text = display + tb->scroll_x;
    uint32_t fg = disabled ? WT_FG_DISABLED : WT_FG;
    font_draw_string_clip(dst, text_x, text_y,
                          vis_text, fg, 13,
                          w->w - PAD_X * 2);

    /* Cursor */
    if (focused && !disabled) {
        int blink_on = (tb->blink_timer / 30) % 2 == 0;
        if (blink_on) {
            int cur_vis = tb->cursor - tb->scroll_x;
            if (cur_vis >= 0) {
                int cx = text_x + cur_vis * CELL_W;
                NvRect cur_r = { cx, text_y, CURSOR_W, CELL_H };
                draw_fill_rect(dst, &cur_r, WT_FG);
            }
        }
        tb->blink_timer++;
    }

    w->flags &= ~WIDGET_DIRTY;
}

/* -----------------------------------------------------------------------
 * Event handling
 * --------------------------------------------------------------------- */

void textbox_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Textbox *tb = (Textbox *)w;
    (void)ud;

    switch (ev->type) {
        case WIDGET_EV_FOCUS_IN:
            tb->blink_timer = 0;
            widget_set_dirty(w);
            break;

        case WIDGET_EV_FOCUS_OUT:
            tb->has_selection = 0;
            widget_set_dirty(w);
            break;

        case WIDGET_EV_MOUSE_DOWN: {
            int vis_x = ev->mouse.x - (w->x + PAD_X);
            int click_col = tb->scroll_x + vis_x / CELL_W;
            if (click_col < 0) click_col = 0;
            if (click_col > tb->len) click_col = tb->len;
            if (ev->mouse.mods & 0x01) { /* shift click */
                if (!tb->has_selection) tb->sel_start = tb->cursor;
                tb->sel_end    = click_col;
                tb->has_selection = (tb->sel_start != tb->sel_end);
            } else {
                tb->cursor    = click_col;
                tb->has_selection = 0;
            }
            widget_set_dirty(w);
            break;
        }

        case WIDGET_EV_KEY_DOWN: {
            uint32_t key  = ev->key.key;
            uint32_t mods = ev->key.mods;
            int ctrl  = (mods & 0x02) != 0;
            int shift = (mods & 0x01) != 0;

            if (ctrl && key == 'a') {
                textbox_select_all(tb);
                break;
            }
            if (ctrl && key == 'c') {
                /* clipboard copy */
                if (tb->has_selection) {
                    int a = tb->sel_start < tb->sel_end ? tb->sel_start : tb->sel_end;
                    int b = tb->sel_start > tb->sel_end ? tb->sel_start : tb->sel_end;
                    char tmp[TEXTBOX_MAX_LEN];
                    int len = b - a;
                    if (len > 0) {
                        strncpy(tmp, tb->text + a, len);
                        tmp[len] = '\0';
                        nv_clipboard_set(tmp);
                    }
                }
                break;
            }
            if (ctrl && key == 'v') {
                char *clip = nv_clipboard_get();
                if (clip) {
                    if (tb->has_selection) delete_selection(tb);
                    for (char *p = clip; *p; p++) insert_char(tb, (uint8_t)*p);
                    free(clip);
                }
                break;
            }
            if (ctrl && key == 'x') {
                if (tb->has_selection) {
                    int a = tb->sel_start < tb->sel_end ? tb->sel_start : tb->sel_end;
                    int b = tb->sel_start > tb->sel_end ? tb->sel_start : tb->sel_end;
                    char tmp[TEXTBOX_MAX_LEN];
                    strncpy(tmp, tb->text + a, b - a);
                    tmp[b - a] = '\0';
                    nv_clipboard_set(tmp);
                    delete_selection(tb);
                }
                break;
            }
            if (ctrl && key == 'z') {
                if (tb->undo_pos > 0) {
                    tb->undo_pos--;
                    strncpy(tb->text, tb->undo_stack[tb->undo_pos],
                            TEXTBOX_MAX_LEN - 1);
                    tb->len    = (int)strlen(tb->text);
                    tb->cursor = tb->undo_cursor[tb->undo_pos];
                    if (tb->cursor > tb->len) tb->cursor = tb->len;
                    tb->has_selection = 0;
                    notify_change(tb);
                }
                break;
            }

            switch (key) {
                case 0xFF51: /* left */
                    if (shift) {
                        if (!tb->has_selection) tb->sel_start = tb->cursor;
                        if (tb->cursor > 0) tb->cursor--;
                        tb->sel_end = tb->cursor;
                        tb->has_selection = (tb->sel_start != tb->sel_end);
                    } else {
                        if (tb->has_selection) {
                            tb->cursor = tb->sel_start < tb->sel_end ?
                                         tb->sel_start : tb->sel_end;
                            tb->has_selection = 0;
                        } else if (tb->cursor > 0) {
                            tb->cursor--;
                        }
                    }
                    ensure_cursor_visible(tb);
                    widget_set_dirty(w);
                    break;
                case 0xFF53: /* right */
                    if (shift) {
                        if (!tb->has_selection) tb->sel_start = tb->cursor;
                        if (tb->cursor < tb->len) tb->cursor++;
                        tb->sel_end = tb->cursor;
                        tb->has_selection = (tb->sel_start != tb->sel_end);
                    } else {
                        if (tb->has_selection) {
                            tb->cursor = tb->sel_start > tb->sel_end ?
                                         tb->sel_start : tb->sel_end;
                            tb->has_selection = 0;
                        } else if (tb->cursor < tb->len) {
                            tb->cursor++;
                        }
                    }
                    ensure_cursor_visible(tb);
                    widget_set_dirty(w);
                    break;
                case 0xFF50: /* home */
                    if (shift) {
                        if (!tb->has_selection) tb->sel_start = tb->cursor;
                        tb->cursor  = 0;
                        tb->sel_end = 0;
                        tb->has_selection = (tb->sel_start != 0);
                    } else {
                        tb->cursor = 0; tb->has_selection = 0;
                    }
                    ensure_cursor_visible(tb);
                    widget_set_dirty(w);
                    break;
                case 0xFF57: /* end */
                    if (shift) {
                        if (!tb->has_selection) tb->sel_start = tb->cursor;
                        tb->cursor  = tb->len;
                        tb->sel_end = tb->len;
                        tb->has_selection = (tb->sel_start != tb->len);
                    } else {
                        tb->cursor = tb->len; tb->has_selection = 0;
                    }
                    ensure_cursor_visible(tb);
                    widget_set_dirty(w);
                    break;
                case 0xFF08: /* backspace */
                    if (!tb->readonly) {
                        if (tb->has_selection) {
                            delete_selection(tb);
                        } else if (tb->cursor > 0) {
                            push_undo(tb);
                            memmove(tb->text + tb->cursor - 1,
                                    tb->text + tb->cursor,
                                    tb->len - tb->cursor + 1);
                            tb->len--;
                            tb->cursor--;
                            ensure_cursor_visible(tb);
                            notify_change(tb);
                        }
                    }
                    break;
                case 0xFFFF: /* delete */
                    if (!tb->readonly) {
                        if (tb->has_selection) {
                            delete_selection(tb);
                        } else if (tb->cursor < tb->len) {
                            push_undo(tb);
                            memmove(tb->text + tb->cursor,
                                    tb->text + tb->cursor + 1,
                                    tb->len - tb->cursor);
                            tb->len--;
                            notify_change(tb);
                        }
                    }
                    break;
                case '\r': case '\n':
                    if (tb->on_enter) tb->on_enter(tb->callback_ud, tb->text);
                    else { WidgetEvent act = { .type = WIDGET_EV_ACTIVATE }; widget_dispatch(w, &act); }
                    break;
                default:
                    if (ev->key.cp >= 32 && ev->key.cp < 127)
                        insert_char(tb, ev->key.cp);
                    break;
            }
            break;
        }

        case WIDGET_EV_TIMER:
            tb->blink_timer++;
            if ((tb->blink_timer % 30) == 0) widget_set_dirty(w);
            break;

        default:
            break;
    }
}   