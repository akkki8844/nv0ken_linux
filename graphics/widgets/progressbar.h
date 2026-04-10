#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include "widget.h"

typedef enum {
    PROGRESS_NORMAL,
    PROGRESS_INDETERMINATE,
    PROGRESS_SUCCESS,
    PROGRESS_ERROR,
} ProgressStyle;

typedef struct {
    Widget        base;
    int           value;
    int           max_value;
    ProgressStyle style;
    int           show_text;
    int           anim_offset;
    int           anim_timer;
    char          label[64];
} Progressbar;

Progressbar *progressbar_new(int x, int y, int w, int h);
void         progressbar_free(Progressbar *pb);
void         progressbar_set_value(Progressbar *pb, int value);
void         progressbar_set_max(Progressbar *pb, int max);
void         progressbar_set_style(Progressbar *pb, ProgressStyle style);
void         progressbar_set_show_text(Progressbar *pb, int show);
void         progressbar_set_label(Progressbar *pb, const char *label);
int          progressbar_value(const Progressbar *pb);
void         progressbar_tick(Progressbar *pb);
void         progressbar_draw(Widget *w, NvSurface *dst);
void         progressbar_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif