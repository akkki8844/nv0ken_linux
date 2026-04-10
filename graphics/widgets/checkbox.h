#ifndef CHECKBOX_H
#define CHECKBOX_H

#include "widget.h"

typedef enum {
    CHECK_UNCHECKED,
    CHECK_CHECKED,
    CHECK_INDETERMINATE,
} CheckState;

typedef struct {
    Widget     base;
    char       label[128];
    CheckState state;
    int        hovered;
    int        pressed;
    int        radio_mode;
    void (*on_change)(void *ud, CheckState state);
    void *change_ud;
} Checkbox;

Checkbox  *checkbox_new(int x, int y, int w, int h, const char *label);
void       checkbox_free(Checkbox *cb);
void       checkbox_set_label(Checkbox *cb, const char *label);
void       checkbox_set_state(Checkbox *cb, CheckState state);
CheckState checkbox_state(const Checkbox *cb);
int        checkbox_checked(const Checkbox *cb);
void       checkbox_set_radio_mode(Checkbox *cb, int radio);
void       checkbox_set_on_change(Checkbox *cb,
                                   void (*fn)(void *, CheckState), void *ud);
void       checkbox_draw(Widget *w, NvSurface *dst);
void       checkbox_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif