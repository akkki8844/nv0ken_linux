#ifndef VT100_H
#define VT100_H

#include <stdint.h>

#define VT_COLOR_DEFAULT  -1

#define VT_ATTR_BOLD          (1 << 0)
#define VT_ATTR_DIM           (1 << 1)
#define VT_ATTR_ITALIC        (1 << 2)
#define VT_ATTR_UNDERLINE     (1 << 3)
#define VT_ATTR_BLINK         (1 << 4)
#define VT_ATTR_REVERSE       (1 << 5)
#define VT_ATTR_HIDDEN        (1 << 6)
#define VT_ATTR_STRIKETHROUGH (1 << 7)

typedef struct {
    uint32_t codepoint;
    int      fg;
    int      bg;
    uint32_t attrs;
} VtCell;

typedef enum {
    VT_STATE_GROUND,
    VT_STATE_ESCAPE,
    VT_STATE_CSI,
    VT_STATE_OSC,
} VtParseState;

typedef struct Vt100 Vt100;

Vt100  *vt100_new(int cols, int rows, int scrollback);
void    vt100_free(Vt100 *v);
void    vt100_resize(Vt100 *v, int cols, int rows);
void    vt100_feed(Vt100 *v, const char *buf, int len);

VtCell *vt100_cell(Vt100 *v, int col, int row);
int     vt100_cursor_col(Vt100 *v);
int     vt100_cursor_row(Vt100 *v);
int     vt100_cursor_visible(Vt100 *v);
int     vt100_cols(Vt100 *v);
int     vt100_rows(Vt100 *v);

VtCell *vt100_scrollback_line(Vt100 *v, int index, int *out_cols);
int     vt100_scrollback_count(Vt100 *v);

#endif