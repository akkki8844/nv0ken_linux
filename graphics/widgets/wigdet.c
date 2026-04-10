#include "widget.h"
#include <stdlib.h>
#include <string.h>

void widget_init(Widget *w, int x, int y, int width, int height) {
    if (!w) return;
    memset(w, 0, sizeof(Widget));
    w->x = x; w->y = y;
    w->w = width; w->h = height;
    w->flags = WIDGET_DIRTY;
}

void widget_free_base(Widget *w) {
    if (!w) return;
    surface_free(w->surface);
    w->surface = NULL;
}

void widget_add_child(Widget *parent, Widget *child) {
    if (!parent || !child || parent->child_count >= WIDGET_MAX_CHILDREN) return;
    child->parent = parent;
    parent->children[parent->child_count++] = child;
}

void widget_remove_child(Widget *parent, Widget *child) {
    if (!parent || !child) return;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            memmove(&parent->children[i], &parent->children[i+1],
                    sizeof(Widget *) * (parent->child_count - i - 1));
            parent->child_count--;
            child->parent = NULL;
            return;
        }
    }
}

void widget_draw(Widget *w, NvSurface *dst) {
    if (!w || (w->flags & WIDGET_HIDDEN)) return;
    if (w->draw) w->draw(w, dst);
    widget_draw_children(w, dst);
}

void widget_draw_children(Widget *w, NvSurface *dst) {
    if (!w) return;
    for (int i = 0; i < w->child_count; i++)
        widget_draw(w->children[i], dst);
}

int widget_hit(const Widget *w, int x, int y) {
    if (!w || (w->flags & WIDGET_HIDDEN)) return 0;
    return x >= w->x && x < w->x + w->w &&
           y >= w->y && y < w->y + w->h;
}

void widget_set_dirty(Widget *w) {
    if (!w) return;
    w->flags |= WIDGET_DIRTY;
    if (w->parent) widget_set_dirty(w->parent);
}

void widget_set_pos(Widget *w, int x, int y) {
    if (!w) return;
    w->x = x; w->y = y;
    widget_set_dirty(w);
}

void widget_set_size(Widget *w, int width, int height) {
    if (!w) return;
    w->w = width; w->h = height;
    widget_set_dirty(w);
}

void widget_set_enabled(Widget *w, int enabled) {
    if (!w) return;
    if (enabled) w->flags &= ~WIDGET_DISABLED;
    else         w->flags |=  WIDGET_DISABLED;
    widget_set_dirty(w);
}

void widget_set_visible(Widget *w, int visible) {
    if (!w) return;
    if (visible) w->flags &= ~WIDGET_HIDDEN;
    else         w->flags |=  WIDGET_HIDDEN;
    widget_set_dirty(w);
}

void widget_set_focused(Widget *w, int focused) {
    if (!w) return;
    if (focused) w->flags |=  WIDGET_FOCUSED;
    else         w->flags &= ~WIDGET_FOCUSED;
    widget_set_dirty(w);
}

void widget_dispatch(Widget *w, const WidgetEvent *ev) {
    if (!w || !ev) return;
    if (w->on_event) w->on_event(w, ev, w->userdata);
}

Widget *widget_find_at(Widget *root, int x, int y) {
    if (!root || (root->flags & WIDGET_HIDDEN)) return NULL;
    for (int i = root->child_count - 1; i >= 0; i--) {
        Widget *hit = widget_find_at(root->children[i], x, y);
        if (hit) return hit;
    }
    if (widget_hit(root, x, y)) return root;
    return NULL;
}