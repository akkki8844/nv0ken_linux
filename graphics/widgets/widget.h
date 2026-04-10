#ifndef WIDGET_H
#define WIDGET_H

#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Widget event types
 * --------------------------------------------------------------------- */

typedef enum {
    WIDGET_EV_NONE,
    WIDGET_EV_MOUSE_DOWN,
    WIDGET_EV_MOUSE_UP,
    WIDGET_EV_MOUSE_MOVE,
    WIDGET_EV_MOUSE_ENTER,
    WIDGET_EV_MOUSE_LEAVE,
    WIDGET_EV_SCROLL,
    WIDGET_EV_KEY_DOWN,
    WIDGET_EV_KEY_UP,
    WIDGET_EV_CHAR,
    WIDGET_EV_FOCUS_IN,
    WIDGET_EV_FOCUS_OUT,
    WIDGET_EV_RESIZE,
    WIDGET_EV_PAINT,
    WIDGET_EV_TIMER,
    WIDGET_EV_CHANGE,
    WIDGET_EV_ACTIVATE,
    WIDGET_EV_CLOSE,
} WidgetEventType;

typedef struct {
    WidgetEventType type;
    union {
        struct { int x, y; int button; uint32_t mods; } mouse;
        struct { int dx, dy; }                          scroll;
        struct { uint32_t key; uint32_t cp; uint32_t mods; } key;
        struct { int w, h; }                            resize;
        struct { int id; }                              timer;
    };
} WidgetEvent;

/* -----------------------------------------------------------------------
 * Widget state flags
 * --------------------------------------------------------------------- */

#define WIDGET_HOVERED  0x01
#define WIDGET_PRESSED  0x02
#define WIDGET_FOCUSED  0x04
#define WIDGET_DISABLED 0x08
#define WIDGET_HIDDEN   0x10
#define WIDGET_DIRTY    0x20

/* -----------------------------------------------------------------------
 * Base widget struct — every widget starts with this
 * --------------------------------------------------------------------- */

#define WIDGET_MAX_CHILDREN 32

typedef struct Widget Widget;

struct Widget {
    int       x, y, w, h;
    int       flags;
    int       tab_order;

    Widget   *parent;
    Widget   *children[WIDGET_MAX_CHILDREN];
    int       child_count;

    NvSurface *surface;

    void (*on_event)(Widget *w, const WidgetEvent *ev, void *userdata);
    void *userdata;

    void (*draw)(Widget *w, NvSurface *dst);
    void (*free)(Widget *w);
};

/* -----------------------------------------------------------------------
 * Base API
 * --------------------------------------------------------------------- */

void widget_init(Widget *w, int x, int y, int width, int height);
void widget_free_base(Widget *w);

void widget_add_child(Widget *parent, Widget *child);
void widget_remove_child(Widget *parent, Widget *child);

void widget_draw(Widget *w, NvSurface *dst);
void widget_draw_children(Widget *w, NvSurface *dst);

int  widget_hit(const Widget *w, int x, int y);
void widget_set_dirty(Widget *w);
void widget_set_pos(Widget *w, int x, int y);
void widget_set_size(Widget *w, int width, int height);
void widget_set_enabled(Widget *w, int enabled);
void widget_set_visible(Widget *w, int visible);
void widget_set_focused(Widget *w, int focused);

void widget_dispatch(Widget *w, const WidgetEvent *ev);
Widget *widget_find_at(Widget *root, int x, int y);

/* -----------------------------------------------------------------------
 * Theme colors — shared across all widgets
 * --------------------------------------------------------------------- */

#define WT_BG              0x1E1E1EFF
#define WT_BG_HOVER        0x2D2D30FF
#define WT_BG_PRESS        0x094771FF
#define WT_BG_DISABLED     0x252526FF
#define WT_BORDER          0x3F3F46FF
#define WT_BORDER_FOCUS    0x007ACCFF
#define WT_BORDER_HOVER    0x555569FF
#define WT_FG              0xCCCCCCFF
#define WT_FG_DIM          0x888888FF
#define WT_FG_DISABLED     0x555555FF
#define WT_ACCENT          0x007ACCFF
#define WT_ACCENT_HOVER    0x1B8AD3FF
#define WT_ACCENT_PRESS    0x005A9EFF
#define WT_SELECTION       0x264F78FF
#define WT_SCROLLBAR_BG    0x252526FF
#define WT_SCROLLBAR_FG    0x424245FF
#define WT_SCROLLBAR_HOVER 0x686868FF
#define WT_INPUT_BG        0x3C3C3CFF
#define WT_SHADOW          0x00000044

#endif