#ifndef BUTTON_H
#define BUTTON_H

#include "widget.h"

typedef enum {
    BTN_STYLE_NORMAL,
    BTN_STYLE_PRIMARY,
    BTN_STYLE_DANGER,
    BTN_STYLE_FLAT,
    BTN_STYLE_ICON,
} ButtonStyle;

typedef struct {
    Widget      base;
    char        label[128];
    ButtonStyle style;
    int         hovered;
    int         pressed;
    int         toggle_mode;
    int         toggled;
    uint32_t    icon_color;
    void      (*on_click)(void *userdata);
    void       *click_userdata;
} Button;

Button *button_new(int x, int y, int w, int h, const char *label,
                   ButtonStyle style);
void    button_free(Button *btn);
void    button_set_label(Button *btn, const char *label);
void    button_set_style(Button *btn, ButtonStyle style);
void    button_set_toggle(Button *btn, int enabled, int initial);
void    button_set_on_click(Button *btn, void (*cb)(void *), void *ud);
int     button_is_toggled(const Button *btn);

void    button_draw(Widget *w, NvSurface *dst);
void    button_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif