#ifndef TEXTAREA_H
#define TEXTAREA_H

#include "widget.h"
#include "scrollbar.h"

#define TEXTAREA_MAX_LINES   4096
#define TEXTAREA_LINE_CAP    1024
#define TEXTAREA_UNDO_DEPTH  64
#define TEXTAREA_TAB_WIDTH   4

/* -----------------------------------------------------------------------
 * A single line of text stored as a dynamic char buffer
 * --------------------------------------------------------------------- */

typedef struct {
    char *buf;
    int   len;
    int   cap;
} TALine;

/* -----------------------------------------------------------------------
 * Cursor / mark position
 * --------------------------------------------------------------------- */

typedef struct {
    int row;
    int col;
} TAPos;

/* -----------------------------------------------------------------------
 * Undo record
 * --------------------------------------------------------------------- */

typedef enum {
    TA_OP_INSERT,
    TA_OP_DELETE,
} TAOpType;

typedef struct {
    TAOpType type;
    TAPos    start;
    TAPos    end;
    char    *text;
    int      text_len;
} TAUndo;

/* -----------------------------------------------------------------------
 * Main struct
 * --------------------------------------------------------------------- */

typedef struct {
    Widget     base;

    TALine     lines[TEXTAREA_MAX_LINES];
    int        line_count;

    TAPos      cursor;
    TAPos      mark;
    int        has_selection;

    int        scroll_row;
    int        scroll_col;

    int        cell_w;
    int        cell_h;
    int        gutter_w;
    int        show_gutter;
    int        show_cursor;
    int        blink_timer;

    int        readonly;
    int        word_wrap;

    TAUndo     undo_stack[TEXTAREA_UNDO_DEPTH];
    int        undo_count;
    int        undo_pos;

    Scrollbar *vscroll;
    Scrollbar *hscroll;

    void (*on_change)(void *ud);
    void (*on_cursor_move)(void *ud, int row, int col);
    void *callback_ud;
} Textarea;

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

Textarea   *textarea_new(int x, int y, int w, int h);
void        textarea_free(Textarea *ta);

/* -----------------------------------------------------------------------
 * Content
 * --------------------------------------------------------------------- */

void        textarea_set_text(Textarea *ta, const char *text);
char       *textarea_get_text(Textarea *ta);
void        textarea_insert_at(Textarea *ta, TAPos pos, const char *text, int len);
void        textarea_delete_range(Textarea *ta, TAPos start, TAPos end);
void        textarea_clear(Textarea *ta);

int         textarea_line_count(const Textarea *ta);
const char *textarea_line_text(const Textarea *ta, int row);
int         textarea_line_len(const Textarea *ta, int row);

/* -----------------------------------------------------------------------
 * Cursor / selection
 * --------------------------------------------------------------------- */

void        textarea_set_cursor(Textarea *ta, int row, int col);
TAPos       textarea_cursor(const Textarea *ta);
void        textarea_select_all(Textarea *ta);
void        textarea_select_range(Textarea *ta, TAPos start, TAPos end);
void        textarea_clear_selection(Textarea *ta);
int         textarea_has_selection(const Textarea *ta);
char       *textarea_selected_text(Textarea *ta);
void        textarea_scroll_to_cursor(Textarea *ta);

/* -----------------------------------------------------------------------
 * Edit operations
 * --------------------------------------------------------------------- */

void        textarea_insert_char(Textarea *ta, uint32_t codepoint);
void        textarea_newline(Textarea *ta);
void        textarea_backspace(Textarea *ta);
void        textarea_delete_fwd(Textarea *ta);
void        textarea_undo(Textarea *ta);
void        textarea_redo(Textarea *ta);
void        textarea_indent(Textarea *ta);
void        textarea_dedent(Textarea *ta);
void        textarea_delete_line(Textarea *ta);
void        textarea_duplicate_line(Textarea *ta);

/* -----------------------------------------------------------------------
 * Display / callbacks
 * --------------------------------------------------------------------- */

void        textarea_set_readonly(Textarea *ta, int ro);
void        textarea_set_show_gutter(Textarea *ta, int show);
void        textarea_set_on_change(Textarea *ta, void (*cb)(void *), void *ud);
void        textarea_set_on_cursor_move(Textarea *ta,
                                         void (*cb)(void *, int, int), void *ud);

void        textarea_draw(Widget *w, NvSurface *dst);
void        textarea_on_event(Widget *w, const WidgetEvent *ev, void *ud);

#endif