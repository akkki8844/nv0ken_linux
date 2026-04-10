#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#include "widget.h"

typedef enum {
    SCROLLBAR_VERTICAL,
    SCROLLBAR_HORIZONTAL,
} ScrollbarOrientation;

typedef struct {
    Widget               base;
    ScrollbarOrientation orient;
    int                  min_val;
    int                  max_val;
    int                  page_size;
    int                  value;
    int                  thumb_pos;
    int                  thumb_size;
    int                  dragging;
    int                  drag_offset;
    int                  hovered_thumb;
    int                  hovered_track;
    void (*on_scroll)(void *ud, int value);
    void *scroll_ud;
} Scrollbar;

Scrollbar *scrollbar_new(int x, int y, int w, int h,
                          ScrollbarOrientation orient);
void       scrollbar_free(Scrollbar *sb);
void       scrollbar_set_range(Scrollbar *sb, int min, int max, int page);
void       scrollbar_set_value(Scrollbar *sb, int value);
int        scrollbar_value(const Scrollbar *sb);
void       scrollbar_set_on_scroll(Scrollbar *sb,
                                    void (*cb)(void *, int), void *ud);
void       scrollbar_draw(Widget *w, NvSurface *dst);
void       scrollbar_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif