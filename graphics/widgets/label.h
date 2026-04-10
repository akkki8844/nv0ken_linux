#ifndef LABEL_H
#define LABEL_H

#include "widget.h"

typedef enum {
    LABEL_ALIGN_LEFT,
    LABEL_ALIGN_CENTER,
    LABEL_ALIGN_RIGHT,
} LabelAlign;

typedef struct {
    Widget     base;
    char       text[512];
    int        font_size;
    uint32_t   color;
    uint32_t   bg_color;
    LabelAlign align;
    int        wrap;
    int        bold;
    int        underline;
} Label;

Label *label_new(int x, int y, int w, int h, const char *text);
void   label_free(Label *lbl);
void   label_set_text(Label *lbl, const char *text);
void   label_set_color(Label *lbl, uint32_t color);
void   label_set_bg(Label *lbl, uint32_t bg);
void   label_set_font_size(Label *lbl, int size);
void   label_set_align(Label *lbl, LabelAlign align);
void   label_set_bold(Label *lbl, int bold);
void   label_set_wrap(Label *lbl, int wrap);

void   label_draw(Widget *w, NvSurface *dst);

#endif