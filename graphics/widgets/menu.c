#include "menu.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>

static int menu_height(const Menu *m) {
    int h = 4;
    for (int i = 0; i < m->item_count; i++)
        h += (m->items[i].type == MENU_ITEM_SEPARATOR) ? 8 : MENU_ITEM_H;
    return h + 4;
}

static int menu_width(const Menu *m) {
    int w = MENU_MIN_W;
    for (int i = 0; i < m->item_count; i++) {
        int tw = (int)strlen(m->items[i].label) * 7 + MENU_PAD_X * 2;
        if (m->items[i].shortcut[0])
            tw += MENU_SHORTCUT_W;
        if (tw > w) w = tw;
    }
    return w;
}

Menu *menu_new(void) {
    Menu *m = malloc(sizeof(Menu));
    if (!m) return NULL;
    memset(m, 0, sizeof(Menu));
    widget_init(&m->base, 0, 0, MENU_MIN_W, 0);
    m->hovered          = -1;
    m->open_submenu_idx = -1;
    m->base.draw        = menu_draw;
    m->base.on_event    = menu_on_event;
    return m;
}

void menu_free(Menu *m) {
    if (!m) return;
    widget_free_base(&m->base);
    free(m);
}

void menu_add_item(Menu *m, const char *label, const char *shortcut,
                   void (*action)(void *), void *ud) {
    if (!m || m->item_count >= MENU_MAX_ITEMS) return;
    MenuItem *item = &m->items[m->item_count++];
    memset(item, 0, sizeof(MenuItem));
    item->type    = MENU_ITEM_NORMAL;
    item->enabled = 1;
    if (label)    strncpy(item->label,    label,    sizeof(item->label)    - 1);
    if (shortcut) strncpy(item->shortcut, shortcut, sizeof(item->shortcut) - 1);
    item->action    = action;
    item->action_ud = ud;
}

void menu_add_check(Menu *m, const char *label, int *checked_ptr) {
    if (!m || m->item_count >= MENU_MAX_ITEMS) return;
    MenuItem *item = &m->items[m->item_count++];
    memset(item, 0, sizeof(MenuItem));
    item->type        = MENU_ITEM_CHECK;
    item->enabled     = 1;
    item->action_ud   = checked_ptr;
    if (label) strncpy(item->label, label, sizeof(item->label) - 1);
    if (checked_ptr) item->checked = *checked_ptr;
}

void menu_add_separator(Menu *m) {
    if (!m || m->item_count >= MENU_MAX_ITEMS) return;
    MenuItem *item = &m->items[m->item_count++];
    memset(item, 0, sizeof(MenuItem));
    item->type = MENU_ITEM_SEPARATOR;
}

MenuItem *menu_add_submenu(Menu *m, const char *label, Menu *submenu) {
    if (!m || m->item_count >= MENU_MAX_ITEMS) return NULL;
    MenuItem *item = &m->items[m->item_count++];
    memset(item, 0, sizeof(MenuItem));
    item->type    = MENU_ITEM_SUBMENU;
    item->enabled = 1;
    item->submenu = submenu;
    if (label) strncpy(item->label, label, sizeof(item->label) - 1);
    if (submenu) submenu->parent_menu = m;
    return item;
}

void menu_show(Menu *m, int x, int y, NvSurface *surface) {
    if (!m) return;
    int w = menu_width(m);
    int h = menu_height(m);
    m->base.x = x;
    m->base.y = y;
    m->base.w = w;
    m->base.h = h;
    m->visible = 1;
    m->hovered = -1;
    widget_set_dirty(&m->base);
    (void)surface;
}

void menu_hide(Menu *m) {
    if (!m) return;
    m->visible = 0;
    m->hovered = -1;
    if (m->open_submenu_idx >= 0) {
        MenuItem *item = &m->items[m->open_submenu_idx];
        if (item->submenu) menu_hide(item->submenu);
        m->open_submenu_idx = -1;
    }
    widget_set_dirty(&m->base);
}

int  menu_visible(const Menu *m)   { return m ? m->visible : 0; }
int  menu_item_count(const Menu *m) { return m ? m->item_count : 0; }

void menu_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Menu *m = (Menu *)w;
    if (!m->visible) return;

    /* shadow */
    NvRect shadow = { w->x + 4, w->y + 4, w->w, w->h };
    draw_fill_rect_alpha(dst, &shadow, 0x00000055);

    NvRect bg = { w->x, w->y, w->w, w->h };
    draw_fill_rect(dst, &bg, 0xF0F0F0FF);
    draw_rect(dst, &bg, 0xBBBBBBFF);

    int iy = w->y + 4;
    for (int i = 0; i < m->item_count; i++) {
        MenuItem *item = &m->items[i];

        if (item->type == MENU_ITEM_SEPARATOR) {
            NvRect sep = { w->x + 4, iy + 3, w->w - 8, 1 };
            draw_fill_rect(dst, &sep, 0xCCCCCCFF);
            iy += 8;
            continue;
        }

        NvRect row = { w->x + 1, iy, w->w - 2, MENU_ITEM_H };

        if (i == m->hovered && item->enabled) {
            draw_fill_rect(dst, &row, 0x0078D7FF);
        }

        uint32_t fg = !item->enabled      ? 0xAAAAAAFF :
                       i == m->hovered    ? 0xFFFFFFFF :
                                            0x111111FF;

        /* check indicator */
        if (item->type == MENU_ITEM_CHECK && item->checked)
            font_draw_string(dst, w->x + 4, iy + (MENU_ITEM_H - 13) / 2,
                             "v", fg, 12);

        font_draw_string(dst, w->x + MENU_PAD_X, iy + (MENU_ITEM_H - 13) / 2,
                         item->label, fg, 13);

        if (item->shortcut[0]) {
            int sw = (int)strlen(item->shortcut) * 6;
            font_draw_string(dst,
                             w->x + w->w - sw - 6,
                             iy + (MENU_ITEM_H - 11) / 2,
                             item->shortcut, i == m->hovered ? 0xCCCCCCFF : 0x888888FF,
                             11);
        }

        if (item->type == MENU_ITEM_SUBMENU) {
            font_draw_string(dst,
                             w->x + w->w - 12,
                             iy + (MENU_ITEM_H - 13) / 2,
                             ">", fg, 12);
        }

        iy += MENU_ITEM_H;
    }

    /* draw open submenu */
    if (m->open_submenu_idx >= 0) {
        MenuItem *item = &m->items[m->open_submenu_idx];
        if (item->submenu) menu_draw(&item->submenu->base, dst);
    }

    w->flags &= ~WIDGET_DIRTY;
}

static int item_at_y(Menu *m, int y) {
    int iy = m->base.y + 4;
    for (int i = 0; i < m->item_count; i++) {
        MenuItem *item = &m->items[i];
        int item_height = (item->type == MENU_ITEM_SEPARATOR) ? 8 : MENU_ITEM_H;
        if (y >= iy && y < iy + item_height)
            return (item->type == MENU_ITEM_SEPARATOR) ? -1 : i;
        iy += item_height;
    }
    return -1;
}

void menu_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Menu *m = (Menu *)w;
    (void)ud;
    if (!m->visible) return;

    switch (ev->type) {
        case WIDGET_EV_MOUSE_MOVE: {
            int prev = m->hovered;
            m->hovered = item_at_y(m, ev->mouse.y);
            if (m->hovered != prev) {
                if (m->open_submenu_idx >= 0 && m->hovered != m->open_submenu_idx) {
                    menu_hide(m->items[m->open_submenu_idx].submenu);
                    m->open_submenu_idx = -1;
                }
                if (m->hovered >= 0 &&
                    m->items[m->hovered].type == MENU_ITEM_SUBMENU) {
                    MenuItem *item = &m->items[m->hovered];
                    int iy = m->base.y + 4;
                    for (int i = 0; i < m->hovered; i++) {
                        iy += (m->items[i].type == MENU_ITEM_SEPARATOR)
                              ? 8 : MENU_ITEM_H;
                    }
                    menu_show(item->submenu,
                               m->base.x + m->base.w,
                               m->base.y + (iy - m->base.y), NULL);
                    m->open_submenu_idx = m->hovered;
                }
                widget_set_dirty(w);
            }
            break;
        }
        case WIDGET_EV_MOUSE_DOWN: {
            if (!widget_hit(w, ev->mouse.x, ev->mouse.y)) {
                menu_hide(m);
                return;
            }
            int idx = item_at_y(m, ev->mouse.y);
            if (idx >= 0 && idx < m->item_count) {
                MenuItem *item = &m->items[idx];
                if (!item->enabled) break;
                if (item->type == MENU_ITEM_NORMAL && item->action) {
                    item->action(item->action_ud);
                    menu_hide(m);
                } else if (item->type == MENU_ITEM_CHECK) {
                    item->checked = !item->checked;
                    if (item->action_ud) *(int*)item->action_ud = item->checked;
                    menu_hide(m);
                }
            }
            widget_set_dirty(w);
            break;
        }
        case WIDGET_EV_KEY_DOWN:
            switch (ev->key.key) {
                case 0xFF52: /* up */
                    if (m->hovered > 0) {
                        m->hovered--;
                        while (m->hovered > 0 &&
                               m->items[m->hovered].type == MENU_ITEM_SEPARATOR)
                            m->hovered--;
                        widget_set_dirty(w);
                    }
                    break;
                case 0xFF54: /* down */
                    if (m->hovered < m->item_count - 1) {
                        m->hovered++;
                        while (m->hovered < m->item_count - 1 &&
                               m->items[m->hovered].type == MENU_ITEM_SEPARATOR)
                            m->hovered++;
                        widget_set_dirty(w);
                    }
                    break;
                case '\r':
                    if (m->hovered >= 0 && m->items[m->hovered].action)
                        m->items[m->hovered].action(m->items[m->hovered].action_ud);
                    menu_hide(m);
                    break;
                case 0xFF1B: /* escape */
                    menu_hide(m);
                    break;
            }
            break;
        default:
            break;
    }
}