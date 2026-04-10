#ifndef LISTVIEW_H
#define LISTVIEW_H

#include "widget.h"
#include "scrollbar.h"

#define LISTVIEW_MAX_ITEMS  1024
#define LISTVIEW_ITEM_H     24
#define LISTVIEW_MAX_COLS   8

typedef struct {
    char     text[256];
    char     cols[LISTVIEW_MAX_COLS][64];
    int      col_count;
    uint32_t color;
    uint32_t bg_color;
    int      selected;
    int      hovered;
    void    *data;
} ListItem;

typedef struct {
    Widget     base;
    ListItem   items[LISTVIEW_MAX_ITEMS];
    int        item_count;
    int        selected;
    int        hovered;
    int        scroll_offset;
    int        item_h;
    int        show_header;
    char       headers[LISTVIEW_MAX_COLS][64];
    int        col_widths[LISTVIEW_MAX_COLS];
    int        col_count;
    int        multi_select;
    Scrollbar *scrollbar;
    void (*on_select)(void *ud, int index);
    void (*on_activate)(void *ud, int index);
    void *list_ud;
} ListView;

ListView *listview_new(int x, int y, int w, int h);
void      listview_free(ListView *lv);
void      listview_clear(ListView *lv);
int       listview_add_item(ListView *lv, const char *text);
void      listview_set_item_text(ListView *lv, int idx, const char *text);
void      listview_set_item_col(ListView *lv, int idx, int col, const char *text);
void      listview_set_item_color(ListView *lv, int idx, uint32_t fg, uint32_t bg);
void      listview_set_item_data(ListView *lv, int idx, void *data);
void      listview_remove_item(ListView *lv, int idx);
void      listview_set_selected(ListView *lv, int idx);
int       listview_selected(const ListView *lv);
ListItem *listview_selected_item(ListView *lv);
void      listview_set_columns(ListView *lv, int count,
                                const char **headers, const int *widths);
void      listview_scroll_to(ListView *lv, int idx);
void      listview_set_on_select(ListView *lv, void (*cb)(void *, int), void *ud);
void      listview_set_on_activate(ListView *lv, void (*cb)(void *, int), void *ud);
void      listview_draw(Widget *w, NvSurface *dst);
void      listview_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif