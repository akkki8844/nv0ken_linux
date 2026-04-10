#ifndef TEXTBOX_H
#define TEXTBOX_H

#include "widget.h"

#define TEXTBOX_MAX_LEN  1024
#define TEXTBOX_HISTORY  32

typedef struct {
    Widget   base;
    char     text[TEXTBOX_MAX_LEN];
    int      len;
    int      cursor;
    int      sel_start;
    int      sel_end;
    int      has_selection;
    int      scroll_x;
    char     placeholder[128];
    int      multiline;
    int      readonly;
    int      password;
    int      max_len;
    int      cursor_blink;
    int      blink_timer;

    char     undo_stack[TEXTBOX_HISTORY][TEXTBOX_MAX_LEN];
    int      undo_cursor[TEXTBOX_HISTORY];
    int      undo_count;
    int      undo_pos;

    void   (*on_change)(void *ud, const char *text);
    void   (*on_enter)(void *ud, const char *text);
    void    *callback_ud;
} Textbox;

Textbox *textbox_new(int x, int y, int w, int h);
void     textbox_free(Textbox *tb);
void     textbox_set_text(Textbox *tb, const char *text);
void     textbox_set_placeholder(Textbox *tb, const char *ph);
void     textbox_set_readonly(Textbox *tb, int ro);
void     textbox_set_password(Textbox *tb, int pw);
void     textbox_set_max_len(Textbox *tb, int max);
void     textbox_set_on_change(Textbox *tb, void (*cb)(void *, const char *), void *ud);
void     textbox_set_on_enter(Textbox *tb, void (*cb)(void *, const char *), void *ud);
const char *textbox_text(const Textbox *tb);
void     textbox_select_all(Textbox *tb);
void     textbox_clear(Textbox *tb);

void     textbox_draw(Widget *w, NvSurface *dst);
void     textbox_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif