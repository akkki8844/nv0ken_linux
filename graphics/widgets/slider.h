#ifndef SLIDER_H
#define SLIDER_H

#include "widget.h"

typedef enum {
    SLIDER_HORIZONTAL,
    SLIDER_VERTICAL,
} SliderOrientation;

typedef struct {
    Widget            base;
    SliderOrientation orient;
    int               min_val;
    int               max_val;
    int               value;
    int               step;
    int               dragging;
    int               drag_offset;
    int               hovered;
    int               show_value;
    char              fmt[16];
    void (*on_change)(void *ud, int value);
    void *change_ud;
} Slider;

Slider *slider_new(int x, int y, int w, int h, SliderOrientation orient);
void    slider_free(Slider *sl);
void    slider_set_range(Slider *sl, int min, int max);
void    slider_set_value(Slider *sl, int value);
void    slider_set_step(Slider *sl, int step);
void    slider_set_show_value(Slider *sl, int show, const char *fmt);
void    slider_set_on_change(Slider *sl, void (*cb)(void *, int), void *ud);
int     slider_value(const Slider *sl);
void    slider_draw(Widget *w, NvSurface *dst);
void    slider_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif