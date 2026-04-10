#include "textarea.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * Layout constants
 * --------------------------------------------------------------------- */

#define CELL_W       7
#define CELL_H       16
#define PAD_X        4
#define PAD_Y        2
#define GUTTER_W     40
#define SB_W         12
#define SB_H         12
#define CURSOR_W     2
#define CURSOR_BLINK 40

/* -----------------------------------------------------------------------
 * Memory helpers
 * --------------------------------------------------------------------- */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    int l = strlen(s);
    char *out = malloc(l + 1);
    if (!out) return NULL;
    memcpy(out, s, l + 1);
    return out;
}

/* -----------------------------------------------------------------------
 * TALine helpers
 * --------------------------------------------------------------------- */

static void line_init(TALine *l) {
    l->buf = xmalloc(64);
    l->len = 0;
    l->cap = 64;
}

static void line_free(TALine *l) {
    free(l->buf);
    l->buf = NULL;
    l->len = l->cap = 0;
}

static void line_ensure(TALine *l, int need) {
    if (need <= l->cap) return;
    int nc = l->cap * 2;
    while (nc < need) nc *= 2;
    char *nb = realloc(l->buf, nc);
    if (!nb) return;
    l->buf = nb;
    l->cap = nc;
}

static void line_insert(TALine *l, int col, const char *text, int len) {
    if (col < 0) col = 0;
    if (col > l->len) col = l->len;
    line_ensure(l, l->len + len + 1);
    memmove(l->buf + col + len, l->buf + col, l->len - col);
    memcpy(l->buf + col, text, len);
    l->len += len;
    l->buf[l->len] = '\0';
}

static void line_delete(TALine *l, int col, int count) {
    if (col < 0) col = 0;
    if (col >= l->len) return;
    if (col + count > l->len) count = l->len - col;
    memmove(l->buf + col, l->buf + col + count, l->len - col - count + 1);
    l->len -= count;
}

/* -----------------------------------------------------------------------
 * Scrollbar callbacks
 * --------------------------------------------------------------------- */

static void on_vscroll(void *ud, int val) {
    Textarea *ta = (Textarea *)ud;
    ta->scroll_row = val;
    widget_set_dirty(&ta->base);
}

static void on_hscroll(void *ud, int val) {
    Textarea *ta = (Textarea *)ud;
    ta->scroll_col = val;
    widget_set_dirty(&ta->base);
}

/* -----------------------------------------------------------------------
 * Scrollbar layout
 * --------------------------------------------------------------------- */

static void layout_scrollbars(Textarea *ta) {
    Widget *w = &ta->base;
    scrollbar_set_range(ta->vscroll, 0, ta->line_count, ta->visible_rows(ta));
    scrollbar_set_range(ta->hscroll, 0, 200, ta->visible_cols(ta));
    ta->vscroll->base.x = w->x + w->w - SB_W;
    ta->vscroll->base.y = w->y;
    ta->vscroll->base.w = SB_W;
    ta->vscroll->base.h = w->h - SB_H;
    ta->hscroll->base.x = w->x;
    ta->hscroll->base.y = w->y + w->h - SB_H;
    ta->hscroll->base.w = w->w - SB_W;
    ta->hscroll->base.h = SB_H;
}

/* local helpers because C doesn't allow method-like calls */
static int visible_rows(Textarea *ta) {
    return (ta->base.h - SB_H - PAD_Y * 2) / CELL_H;
}
static int visible_cols(Textarea *ta) {
    int gw = ta->show_gutter ? GUTTER_W : 0;
    return (ta->base.w - SB_W - gw - PAD_X * 2) / CELL_W;
}
static int content_x(Textarea *ta) {
    return ta->base.x + (ta->show_gutter ? GUTTER_W : 0) + PAD_X;
}
static int content_y(Textarea *ta) {
    return ta->base.y + PAD_Y;
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

Textarea *textarea_new(int x, int y, int w, int h) {
    Textarea *ta = xmalloc(sizeof(Textarea));
    if (!ta) return NULL;
    widget_init(&ta->base, x, y, w, h);
    ta->cell_w       = CELL_W;
    ta->cell_h       = CELL_H;
    ta->gutter_w     = GUTTER_W;
    ta->show_gutter  = 1;
    ta->show_cursor  = 1;
    ta->blink_timer  = 0;

    /* seed with one empty line */
    line_init(&ta->lines[0]);
    ta->line_count = 1;

    ta->cursor.row = 0; ta->cursor.col = 0;
    ta->mark.row   = 0; ta->mark.col   = 0;

    ta->vscroll = scrollbar_new(x + w - SB_W, y, SB_W, h - SB_H,
                                 SCROLLBAR_VERTICAL);
    ta->hscroll = scrollbar_new(x, y + h - SB_H, w - SB_W, SB_H,
                                 SCROLLBAR_HORIZONTAL);
    scrollbar_set_on_scroll(ta->vscroll, on_vscroll, ta);
    scrollbar_set_on_scroll(ta->hscroll, on_hscroll, ta);

    widget_add_child(&ta->base, &ta->vscroll->base);
    widget_add_child(&ta->base, &ta->hscroll->base);

    ta->base.draw     = textarea_draw;
    ta->base.on_event = textarea_on_event;
    return ta;
}

void textarea_free(Textarea *ta) {
    if (!ta) return;
    for (int i = 0; i < ta->line_count; i++)
        line_free(&ta->lines[i]);
    for (int i = 0; i < ta->undo_count; i++)
        free(ta->undo_stack[i].text);
    scrollbar_free(ta->vscroll);
    scrollbar_free(ta->hscroll);
    widget_free_base(&ta->base);
    free(ta);
}

/* -----------------------------------------------------------------------
 * Content management
 * --------------------------------------------------------------------- */

void textarea_clear(Textarea *ta) {
    if (!ta) return;
    for (int i = 0; i < ta->line_count; i++) line_free(&ta->lines[i]);
    line_init(&ta->lines[0]);
    ta->line_count = 1;
    ta->cursor.row = ta->cursor.col = 0;
    ta->mark = ta->cursor;
    ta->has_selection = 0;
    ta->scroll_row = ta->scroll_col = 0;
    scrollbar_set_value(ta->vscroll, 0);
    scrollbar_set_value(ta->hscroll, 0);
    widget_set_dirty(&ta->base);
}

void textarea_set_text(Textarea *ta, const char *text) {
    if (!ta) return;
    textarea_clear(ta);
    if (!text || !text[0]) return;

    const char *p = text;
    int row = 0;
    while (*p) {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (row >= TEXTAREA_MAX_LINES) break;
        if (row > 0) {
            line_init(&ta->lines[row]);
            ta->line_count = row + 1;
        }
        line_insert(&ta->lines[row], 0, p, len);
        p += len;
        if (*p == '\n') { p++; row++; }
        else break;
    }
    ta->cursor.row = 0; ta->cursor.col = 0;
    scrollbar_set_range(ta->vscroll, 0, ta->line_count,
                        visible_rows(ta));
    widget_set_dirty(&ta->base);
}

char *textarea_get_text(Textarea *ta) {
    if (!ta) return xstrdup("");
    int total = 0;
    for (int i = 0; i < ta->line_count; i++)
        total += ta->lines[i].len + 1;
    char *out = malloc(total + 1);
    if (!out) return NULL;
    int pos = 0;
    for (int i = 0; i < ta->line_count; i++) {
        memcpy(out + pos, ta->lines[i].buf, ta->lines[i].len);
        pos += ta->lines[i].len;
        if (i < ta->line_count - 1) out[pos++] = '\n';
    }
    out[pos] = '\0';
    return out;
}

int textarea_line_count(const Textarea *ta) { return ta ? ta->line_count : 0; }

const char *textarea_line_text(const Textarea *ta, int row) {
    if (!ta || row < 0 || row >= ta->line_count) return "";
    return ta->lines[row].buf ? ta->lines[row].buf : "";
}

int textarea_line_len(const Textarea *ta, int row) {
    if (!ta || row < 0 || row >= ta->line_count) return 0;
    return ta->lines[row].len;
}

/* -----------------------------------------------------------------------
 * Clamp helpers
 * --------------------------------------------------------------------- */

static void clamp_pos(Textarea *ta, TAPos *p) {
    if (p->row < 0) p->row = 0;
    if (p->row >= ta->line_count) p->row = ta->line_count - 1;
    if (p->col < 0) p->col = 0;
    int llen = ta->lines[p->row].len;
    if (p->col > llen) p->col = llen;
}

static int pos_lt(TAPos a, TAPos b) {
    return a.row < b.row || (a.row == b.row && a.col < b.col);
}

static TAPos pos_min(TAPos a, TAPos b) { return pos_lt(a, b) ? a : b; }
static TAPos pos_max(TAPos a, TAPos b) { return pos_lt(a, b) ? b : a; }

/* -----------------------------------------------------------------------
 * Undo / redo
 * --------------------------------------------------------------------- */

static void undo_push(Textarea *ta, TAOpType type, TAPos start, TAPos end,
                       const char *text, int len) {
    if (ta->undo_pos < ta->undo_count) {
        for (int i = ta->undo_pos; i < ta->undo_count; i++)
            free(ta->undo_stack[i].text);
        ta->undo_count = ta->undo_pos;
    }
    if (ta->undo_count >= TEXTAREA_UNDO_DEPTH) {
        free(ta->undo_stack[0].text);
        memmove(&ta->undo_stack[0], &ta->undo_stack[1],
                sizeof(TAUndo) * (TEXTAREA_UNDO_DEPTH - 1));
        ta->undo_count--;
    }
    TAUndo *u = &ta->undo_stack[ta->undo_count++];
    u->type     = type;
    u->start    = start;
    u->end      = end;
    u->text     = malloc(len + 1);
    u->text_len = len;
    if (u->text) { memcpy(u->text, text, len); u->text[len] = '\0'; }
    ta->undo_pos = ta->undo_count;
}

void textarea_undo(Textarea *ta) {
    if (!ta || ta->undo_pos <= 0) return;
    ta->undo_pos--;
    TAUndo *u = &ta->undo_stack[ta->undo_pos];
    if (u->type == TA_OP_INSERT) {
        textarea_delete_range(ta, u->start, u->end);
    } else {
        textarea_insert_at(ta, u->start, u->text, u->text_len);
    }
    ta->cursor = u->start;
    clamp_pos(ta, &ta->cursor);
    textarea_scroll_to_cursor(ta);
    widget_set_dirty(&ta->base);
}

void textarea_redo(Textarea *ta) {
    if (!ta || ta->undo_pos >= ta->undo_count) return;
    TAUndo *u = &ta->undo_stack[ta->undo_pos];
    if (u->type == TA_OP_INSERT) {
        textarea_insert_at(ta, u->start, u->text, u->text_len);
        ta->cursor = u->end;
    } else {
        textarea_delete_range(ta, u->start, u->end);
        ta->cursor = u->start;
    }
    ta->undo_pos++;
    clamp_pos(ta, &ta->cursor);
    textarea_scroll_to_cursor(ta);
    widget_set_dirty(&ta->base);
}

/* -----------------------------------------------------------------------
 * Core insert / delete (no undo recording — called internally)
 * --------------------------------------------------------------------- */

void textarea_insert_at(Textarea *ta, TAPos pos, const char *text, int len) {
    if (!ta || !text || len <= 0) return;
    clamp_pos(ta, &pos);

    int row = pos.row;
    int col = pos.col;

    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c == '\n') {
            /* split line at col */
            if (row + 1 >= TEXTAREA_MAX_LINES) break;
            memmove(&ta->lines[row + 2], &ta->lines[row + 1],
                    sizeof(TALine) * (ta->line_count - row - 1));
            ta->line_count++;
            line_init(&ta->lines[row + 1]);

            int tail = ta->lines[row].len - col;
            if (tail > 0) {
                line_insert(&ta->lines[row + 1], 0,
                            ta->lines[row].buf + col, tail);
                line_delete(&ta->lines[row], col, tail);
            }
            row++;
            col = 0;
        } else {
            line_insert(&ta->lines[row], col, &c, 1);
            col++;
        }
    }

    scrollbar_set_range(ta->vscroll, 0, ta->line_count, visible_rows(ta));
    widget_set_dirty(&ta->base);
}

void textarea_delete_range(Textarea *ta, TAPos start, TAPos end) {
    if (!ta) return;
    clamp_pos(ta, &start);
    clamp_pos(ta, &end);
    if (!pos_lt(start, end)) return;

    if (start.row == end.row) {
        line_delete(&ta->lines[start.row], start.col, end.col - start.col);
        return;
    }

    /* remove partial first line tail */
    int first_tail = ta->lines[start.row].len - start.col;
    line_delete(&ta->lines[start.row], start.col, first_tail);

    /* append partial last line head to first line */
    if (end.col < ta->lines[end.row].len) {
        line_insert(&ta->lines[start.row], start.col,
                    ta->lines[end.row].buf + end.col,
                    ta->lines[end.row].len - end.col);
    }

    /* free middle lines and last line */
    for (int r = start.row + 1; r <= end.row; r++)
        line_free(&ta->lines[r]);

    int removed = end.row - start.row;
    memmove(&ta->lines[start.row + 1], &ta->lines[end.row + 1],
            sizeof(TALine) * (ta->line_count - end.row - 1));
    ta->line_count -= removed;

    scrollbar_set_range(ta->vscroll, 0, ta->line_count, visible_rows(ta));
    widget_set_dirty(&ta->base);
}

/* -----------------------------------------------------------------------
 * High-level edit ops (with undo)
 * --------------------------------------------------------------------- */

void textarea_insert_char(Textarea *ta, uint32_t cp) {
    if (!ta || ta->readonly || cp == '\r') return;
    if (ta->has_selection) {
        TAPos start = pos_min(ta->cursor, ta->mark);
        TAPos end   = pos_max(ta->cursor, ta->mark);
        char *sel   = textarea_selected_text(ta);
        int   slen  = sel ? strlen(sel) : 0;
        if (sel) undo_push(ta, TA_OP_DELETE, start, end, sel, slen);
        free(sel);
        textarea_delete_range(ta, start, end);
        ta->cursor = start;
        ta->has_selection = 0;
    }

    char c  = (char)cp;
    TAPos before = ta->cursor;
    undo_push(ta, TA_OP_INSERT, before,
              (TAPos){ before.row, before.col + 1 }, &c, 1);
    line_insert(&ta->lines[ta->cursor.row], ta->cursor.col, &c, 1);
    ta->cursor.col++;

    textarea_scroll_to_cursor(ta);
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_newline(Textarea *ta) {
    if (!ta || ta->readonly) return;
    if (ta->has_selection) {
        TAPos start = pos_min(ta->cursor, ta->mark);
        TAPos end   = pos_max(ta->cursor, ta->mark);
        textarea_delete_range(ta, start, end);
        ta->cursor = start; ta->has_selection = 0;
    }

    /* auto-indent: count leading spaces on current line */
    int indent = 0;
    TALine *cur = &ta->lines[ta->cursor.row];
    while (indent < cur->len && cur->buf[indent] == ' ') indent++;
    char prev = ta->cursor.col > 0 ? cur->buf[ta->cursor.col - 1] : '\0';
    if (prev == '{' || prev == ':' || prev == '(') indent += TEXTAREA_TAB_WIDTH;

    char buf[TEXTAREA_LINE_CAP];
    buf[0] = '\n';
    memset(buf + 1, ' ', indent);
    int total = 1 + indent;

    TAPos before = ta->cursor;
    TAPos after  = { before.row + 1, indent };
    undo_push(ta, TA_OP_INSERT, before, after, buf, total);
    textarea_insert_at(ta, before, buf, total);
    ta->cursor = after;

    textarea_scroll_to_cursor(ta);
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_backspace(Textarea *ta) {
    if (!ta || ta->readonly) return;
    if (ta->has_selection) {
        TAPos start = pos_min(ta->cursor, ta->mark);
        TAPos end   = pos_max(ta->cursor, ta->mark);
        char *sel   = textarea_selected_text(ta);
        if (sel) undo_push(ta, TA_OP_DELETE, start, end, sel, strlen(sel));
        free(sel);
        textarea_delete_range(ta, start, end);
        ta->cursor = start; ta->has_selection = 0;
        textarea_scroll_to_cursor(ta);
        widget_set_dirty(&ta->base);
        return;
    }
    TAPos pos = ta->cursor;
    if (pos.col == 0 && pos.row == 0) return;

    TAPos prev_pos;
    char deleted[1] = { '\n' };
    if (pos.col > 0) {
        deleted[0] = ta->lines[pos.row].buf[pos.col - 1];
        prev_pos = (TAPos){ pos.row, pos.col - 1 };
        undo_push(ta, TA_OP_DELETE, prev_pos, pos, deleted, 1);
        line_delete(&ta->lines[pos.row], pos.col - 1, 1);
        ta->cursor.col--;
    } else {
        prev_pos = (TAPos){ pos.row - 1, ta->lines[pos.row - 1].len };
        undo_push(ta, TA_OP_DELETE, prev_pos, pos, deleted, 1);
        TAPos end = { pos.row, 0 };
        textarea_delete_range(ta, prev_pos, end);
        ta->cursor = prev_pos;
    }
    textarea_scroll_to_cursor(ta);
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_delete_fwd(Textarea *ta) {
    if (!ta || ta->readonly) return;
    if (ta->has_selection) { textarea_backspace(ta); return; }
    TAPos pos = ta->cursor;
    if (pos.row == ta->line_count - 1 &&
        pos.col == ta->lines[pos.row].len) return;

    TAPos next_pos;
    char deleted[1] = { '\n' };
    if (pos.col < ta->lines[pos.row].len) {
        deleted[0] = ta->lines[pos.row].buf[pos.col];
        next_pos = (TAPos){ pos.row, pos.col + 1 };
        undo_push(ta, TA_OP_DELETE, pos, next_pos, deleted, 1);
        line_delete(&ta->lines[pos.row], pos.col, 1);
    } else {
        next_pos = (TAPos){ pos.row + 1, 0 };
        undo_push(ta, TA_OP_DELETE, pos, next_pos, deleted, 1);
        textarea_delete_range(ta, pos, next_pos);
    }
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_indent(Textarea *ta) {
    if (!ta || ta->readonly) return;
    int start_row = ta->cursor.row;
    int end_row   = ta->has_selection ? ta->mark.row : ta->cursor.row;
    if (start_row > end_row) { int t = start_row; start_row = end_row; end_row = t; }
    char spaces[TEXTAREA_TAB_WIDTH + 1];
    memset(spaces, ' ', TEXTAREA_TAB_WIDTH);
    spaces[TEXTAREA_TAB_WIDTH] = '\0';
    for (int r = start_row; r <= end_row; r++) {
        line_insert(&ta->lines[r], 0, spaces, TEXTAREA_TAB_WIDTH);
    }
    ta->cursor.col += TEXTAREA_TAB_WIDTH;
    if (ta->has_selection) ta->mark.col += TEXTAREA_TAB_WIDTH;
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_dedent(Textarea *ta) {
    if (!ta || ta->readonly) return;
    int start_row = ta->cursor.row;
    int end_row   = ta->has_selection ? ta->mark.row : ta->cursor.row;
    if (start_row > end_row) { int t = start_row; start_row = end_row; end_row = t; }
    for (int r = start_row; r <= end_row; r++) {
        TALine *l = &ta->lines[r];
        int rm = 0;
        while (rm < TEXTAREA_TAB_WIDTH && l->buf[rm] == ' ') rm++;
        if (rm > 0) line_delete(l, 0, rm);
    }
    if (ta->cursor.col > 0) ta->cursor.col -= TEXTAREA_TAB_WIDTH;
    if (ta->cursor.col < 0) ta->cursor.col = 0;
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_delete_line(Textarea *ta) {
    if (!ta || ta->readonly || ta->line_count <= 1) return;
    int r = ta->cursor.row;
    line_free(&ta->lines[r]);
    memmove(&ta->lines[r], &ta->lines[r + 1],
            sizeof(TALine) * (ta->line_count - r - 1));
    ta->line_count--;
    if (ta->cursor.row >= ta->line_count)
        ta->cursor.row = ta->line_count - 1;
    ta->cursor.col = 0;
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

void textarea_duplicate_line(Textarea *ta) {
    if (!ta || ta->readonly || ta->line_count >= TEXTAREA_MAX_LINES - 1) return;
    int r = ta->cursor.row;
    memmove(&ta->lines[r + 2], &ta->lines[r + 1],
            sizeof(TALine) * (ta->line_count - r - 1));
    ta->line_count++;
    line_init(&ta->lines[r + 1]);
    line_insert(&ta->lines[r + 1], 0, ta->lines[r].buf, ta->lines[r].len);
    ta->cursor.row++;
    if (ta->on_change) ta->on_change(ta->callback_ud);
    widget_set_dirty(&ta->base);
}

/* -----------------------------------------------------------------------
 * Cursor / selection
 * --------------------------------------------------------------------- */

void textarea_set_cursor(Textarea *ta, int row, int col) {
    if (!ta) return;
    ta->cursor.row = row;
    ta->cursor.col = col;
    clamp_pos(ta, &ta->cursor);
    ta->has_selection = 0;
    textarea_scroll_to_cursor(ta);
    if (ta->on_cursor_move)
        ta->on_cursor_move(ta->callback_ud, ta->cursor.row, ta->cursor.col);
    widget_set_dirty(&ta->base);
}

TAPos textarea_cursor(const Textarea *ta) {
    return ta ? ta->cursor : (TAPos){0,0};
}

void textarea_select_all(Textarea *ta) {
    if (!ta) return;
    ta->mark.row = 0; ta->mark.col = 0;
    ta->cursor.row = ta->line_count - 1;
    ta->cursor.col = ta->lines[ta->cursor.row].len;
    ta->has_selection = 1;
    widget_set_dirty(&ta->base);
}

void textarea_select_range(Textarea *ta, TAPos start, TAPos end) {
    if (!ta) return;
    clamp_pos(ta, &start); clamp_pos(ta, &end);
    ta->mark   = start;
    ta->cursor = end;
    ta->has_selection = !(!pos_lt(start, end) && !pos_lt(end, start));
    widget_set_dirty(&ta->base);
}

void textarea_clear_selection(Textarea *ta) {
    if (ta) { ta->has_selection = 0; widget_set_dirty(&ta->base); }
}

int textarea_has_selection(const Textarea *ta) {
    return ta ? ta->has_selection : 0;
}

char *textarea_selected_text(Textarea *ta) {
    if (!ta || !ta->has_selection) return xstrdup("");
    TAPos start = pos_min(ta->cursor, ta->mark);
    TAPos end   = pos_max(ta->cursor, ta->mark);

    /* compute length */
    int total = 0;
    for (int r = start.row; r <= end.row; r++) {
        int c0 = (r == start.row) ? start.col : 0;
        int c1 = (r == end.row)   ? end.col   : ta->lines[r].len;
        total += c1 - c0;
        if (r < end.row) total++;
    }

    char *out = malloc(total + 1);
    if (!out) return NULL;
    int pos = 0;
    for (int r = start.row; r <= end.row; r++) {
        int c0 = (r == start.row) ? start.col : 0;
        int c1 = (r == end.row)   ? end.col   : ta->lines[r].len;
        int len = c1 - c0;
        if (len > 0) memcpy(out + pos, ta->lines[r].buf + c0, len);
        pos += len;
        if (r < end.row) out[pos++] = '\n';
    }
    out[pos] = '\0';
    return out;
}

void textarea_scroll_to_cursor(Textarea *ta) {
    if (!ta) return;
    int vr = visible_rows(ta);
    int vc = visible_cols(ta);
    if (vr < 1) vr = 1;
    if (vc < 1) vc = 1;

    if (ta->cursor.row < ta->scroll_row)
        ta->scroll_row = ta->cursor.row;
    if (ta->cursor.row >= ta->scroll_row + vr)
        ta->scroll_row = ta->cursor.row - vr + 1;

    if (ta->cursor.col < ta->scroll_col)
        ta->scroll_col = ta->cursor.col;
    if (ta->cursor.col >= ta->scroll_col + vc)
        ta->scroll_col = ta->cursor.col - vc + 1;

    scrollbar_set_value(ta->vscroll, ta->scroll_row);
    scrollbar_set_value(ta->hscroll, ta->scroll_col);
}

/* -----------------------------------------------------------------------
 * Configuration
 * --------------------------------------------------------------------- */

void textarea_set_readonly(Textarea *ta, int ro)        { if (ta) ta->readonly    = ro; }
void textarea_set_show_gutter(Textarea *ta, int show)   { if (ta) ta->show_gutter = show; widget_set_dirty(&ta->base); }
void textarea_set_on_change(Textarea *ta, void (*cb)(void *), void *ud) {
    if (ta) { ta->on_change = cb; ta->callback_ud = ud; }
}
void textarea_set_on_cursor_move(Textarea *ta, void (*cb)(void *, int, int), void *ud) {
    if (ta) { ta->on_cursor_move = cb; ta->callback_ud = ud; }
}

/* -----------------------------------------------------------------------
 * Draw
 * --------------------------------------------------------------------- */

void textarea_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Textarea *ta = (Textarea *)w;
    int focused  = (w->flags & WIDGET_FOCUSED);

    /* background */
    NvRect bg = { w->x, w->y, w->w, w->h };
    draw_fill_rect(dst, &bg, WT_INPUT_BG);
    draw_rect(dst, &bg, focused ? WT_BORDER_FOCUS : WT_BORDER);

    int gw  = ta->show_gutter ? GUTTER_W : 0;
    int cx0 = w->x + gw + PAD_X;
    int cy0 = w->y + PAD_Y;
    int vr  = visible_rows(ta);
    int vc  = visible_cols(ta);

    TAPos sel_start = {0,0}, sel_end = {0,0};
    if (ta->has_selection) {
        sel_start = pos_min(ta->cursor, ta->mark);
        sel_end   = pos_max(ta->cursor, ta->mark);
    }

    /* current-line highlight */
    if (focused) {
        int ly = cy0 + (ta->cursor.row - ta->scroll_row) * CELL_H;
        if (ly >= cy0 && ly < w->y + w->h - SB_H) {
            NvRect hl = { w->x + gw, ly, w->w - gw - SB_W, CELL_H };
            draw_fill_rect(dst, &hl, 0x28282AFF);
        }
    }

    /* lines */
    for (int vi = 0; vi < vr; vi++) {
        int row = vi + ta->scroll_row;
        if (row >= ta->line_count) break;
        TALine *line = &ta->lines[row];
        int ly = cy0 + vi * CELL_H;
        if (ly + CELL_H > w->y + w->h - SB_H) break;

        /* selection background on this row */
        if (ta->has_selection &&
            row >= sel_start.row && row <= sel_end.row) {
            int sc = (row == sel_start.row) ? sel_start.col : 0;
            int ec = (row == sel_end.row)   ? sel_end.col   : line->len;
            sc -= ta->scroll_col; ec -= ta->scroll_col;
            if (sc < 0) sc = 0;
            if (ec > vc) ec = vc;
            if (sc < ec) {
                NvRect sel = { cx0 + sc * CELL_W, ly,
                               (ec - sc) * CELL_W, CELL_H };
                draw_fill_rect(dst, &sel, WT_SELECTION);
            }
        }

        /* text */
        const char *vis = line->buf + ta->scroll_col;
        if (ta->scroll_col > line->len) vis = "";
        font_draw_string_clip(dst, cx0, ly + 1, vis, WT_FG, 13,
                              w->w - gw - SB_W - PAD_X * 2);

        /* gutter */
        if (ta->show_gutter) {
            NvRect gutter = { w->x, ly, GUTTER_W - 1, CELL_H };
            draw_fill_rect(dst, &gutter, 0x252526FF);
            char num[8];
            snprintf(num, sizeof(num), "%d", row + 1);
            int nw = strlen(num) * 6;
            uint32_t gfg = (row == ta->cursor.row) ? WT_FG : WT_FG_DIM;
            font_draw_string(dst, w->x + GUTTER_W - nw - 4, ly + 1, num, gfg, 11);
        }
    }

    /* gutter border */
    if (ta->show_gutter) {
        NvRect gb = { w->x + GUTTER_W - 1, w->y, 1, w->h - SB_H };
        draw_fill_rect(dst, &gb, WT_BORDER);
    }

    /* cursor */
    if (focused && ta->show_cursor) {
        int blink_on = (ta->blink_timer / CURSOR_BLINK) % 2 == 0;
        if (blink_on) {
            int vrow = ta->cursor.row - ta->scroll_row;
            int vcol = ta->cursor.col - ta->scroll_col;
            if (vrow >= 0 && vrow < vr && vcol >= 0 && vcol <= vc) {
                int cur_x = cx0 + vcol * CELL_W;
                int cur_y = cy0 + vrow * CELL_H;
                NvRect cur = { cur_x, cur_y, CURSOR_W, CELL_H };
                draw_fill_rect(dst, &cur, WT_FG);
            }
        }
        ta->blink_timer++;
    }

    w->flags &= ~WIDGET_DIRTY;
}

/* -----------------------------------------------------------------------
 * Event handling
 * --------------------------------------------------------------------- */

static void move_cursor(Textarea *ta, int dr, int dc, int shift) {
    TAPos prev = ta->cursor;
    if (!shift) ta->has_selection = 0;
    else if (!ta->has_selection) ta->mark = ta->cursor;

    ta->cursor.row += dr;
    if (ta->cursor.row < 0)                   ta->cursor.row = 0;
    if (ta->cursor.row >= ta->line_count)     ta->cursor.row = ta->line_count - 1;

    if (dc != 0) {
        ta->cursor.col += dc;
        if (ta->cursor.col < 0) {
            if (ta->cursor.row > 0) {
                ta->cursor.row--;
                ta->cursor.col = ta->lines[ta->cursor.row].len;
            } else {
                ta->cursor.col = 0;
            }
        }
        int llen = ta->lines[ta->cursor.row].len;
        if (ta->cursor.col > llen) {
            if (ta->cursor.row < ta->line_count - 1) {
                ta->cursor.row++;
                ta->cursor.col = 0;
            } else {
                ta->cursor.col = llen;
            }
        }
    } else if (dr != 0) {
        int llen = ta->lines[ta->cursor.row].len;
        if (ta->cursor.col > llen) ta->cursor.col = llen;
    }

    if (shift) {
        ta->has_selection = !(ta->cursor.row == ta->mark.row &&
                              ta->cursor.col == ta->mark.col);
    }

    (void)prev;
    textarea_scroll_to_cursor(ta);
    if (ta->on_cursor_move)
        ta->on_cursor_move(ta->callback_ud, ta->cursor.row, ta->cursor.col);
    widget_set_dirty(&ta->base);
}

void textarea_on_event(Widget *w, const WidgetEvent *ev, void *ud) {
    if (!w || !ev) return;
    Textarea *ta = (Textarea *)w;
    (void)ud;
    int focused = (w->flags & WIDGET_FOCUSED);

    switch (ev->type) {
        case WIDGET_EV_FOCUS_IN:
            ta->blink_timer = 0;
            widget_set_dirty(w);
            break;
        case WIDGET_EV_FOCUS_OUT:
            ta->has_selection = 0;
            widget_set_dirty(w);
            break;

        case WIDGET_EV_MOUSE_DOWN: {
            if (widget_hit(&ta->vscroll->base, ev->mouse.x, ev->mouse.y)) {
                widget_dispatch(&ta->vscroll->base, ev); break;
            }
            if (widget_hit(&ta->hscroll->base, ev->mouse.x, ev->mouse.y)) {
                widget_dispatch(&ta->hscroll->base, ev); break;
            }
            int gw  = ta->show_gutter ? GUTTER_W : 0;
            int cx0 = w->x + gw + PAD_X;
            int cy0 = w->y + PAD_Y;
            int row = ta->scroll_row + (ev->mouse.y - cy0) / CELL_H;
            int col = ta->scroll_col + (ev->mouse.x - cx0) / CELL_W;
            if (row < 0) row = 0;
            if (row >= ta->line_count) row = ta->line_count - 1;
            if (col < 0) col = 0;
            int llen = ta->lines[row].len;
            if (col > llen) col = llen;
            if (ev->mouse.mods & 0x01) {
                if (!ta->has_selection) ta->mark = ta->cursor;
                ta->cursor.row = row;
                ta->cursor.col = col;
                ta->has_selection = 1;
            } else {
                ta->cursor.row = row;
                ta->cursor.col = col;
                ta->has_selection = 0;
            }
            textarea_scroll_to_cursor(ta);
            widget_set_dirty(w);
            break;
        }

        case WIDGET_EV_MOUSE_MOVE:
            if (widget_hit(&ta->vscroll->base, ev->mouse.x, ev->mouse.y))
                widget_dispatch(&ta->vscroll->base, ev);
            if (widget_hit(&ta->hscroll->base, ev->mouse.x, ev->mouse.y))
                widget_dispatch(&ta->hscroll->base, ev);
            break;

        case WIDGET_EV_MOUSE_UP:
            widget_dispatch(&ta->vscroll->base, ev);
            widget_dispatch(&ta->hscroll->base, ev);
            break;

        case WIDGET_EV_SCROLL:
            ta->scroll_row += ev->scroll.dy * 3;
            if (ta->scroll_row < 0) ta->scroll_row = 0;
            int max_sr = ta->line_count - visible_rows(ta);
            if (max_sr < 0) max_sr = 0;
            if (ta->scroll_row > max_sr) ta->scroll_row = max_sr;
            scrollbar_set_value(ta->vscroll, ta->scroll_row);
            widget_set_dirty(w);
            break;

        case WIDGET_EV_KEY_DOWN: {
            if (!focused) break;
            uint32_t key  = ev->key.key;
            uint32_t mods = ev->key.mods;
            int ctrl  = (mods & 0x02) != 0;
            int shift = (mods & 0x01) != 0;

            if (ctrl && key == 'a') { textarea_select_all(ta); break; }
            if (ctrl && key == 'z') { textarea_undo(ta); break; }
            if (ctrl && key == 'y') { textarea_redo(ta); break; }
            if (ctrl && key == 'c') {
                char *sel = textarea_selected_text(ta);
                if (sel && sel[0]) nv_clipboard_set(sel);
                free(sel);
                break;
            }
            if (ctrl && key == 'x') {
                char *sel = textarea_selected_text(ta);
                if (sel && sel[0]) {
                    nv_clipboard_set(sel);
                    TAPos s = pos_min(ta->cursor, ta->mark);
                    TAPos e = pos_max(ta->cursor, ta->mark);
                    textarea_delete_range(ta, s, e);
                    ta->cursor = s; ta->has_selection = 0;
                    textarea_scroll_to_cursor(ta);
                    if (ta->on_change) ta->on_change(ta->callback_ud);
                }
                free(sel);
                break;
            }
            if (ctrl && key == 'v') {
                char *clip = nv_clipboard_get();
                if (clip) {
                    if (ta->has_selection) {
                        TAPos s = pos_min(ta->cursor, ta->mark);
                        TAPos e = pos_max(ta->cursor, ta->mark);
                        textarea_delete_range(ta, s, e);
                        ta->cursor = s; ta->has_selection = 0;
                    }
                    textarea_insert_at(ta, ta->cursor, clip, strlen(clip));
                    /* move cursor to end of paste */
                    const char *p = clip;
                    while (*p) {
                        if (*p == '\n') { ta->cursor.row++; ta->cursor.col = 0; }
                        else ta->cursor.col++;
                        p++;
                    }
                    free(clip);
                    clamp_pos(ta, &ta->cursor);
                    textarea_scroll_to_cursor(ta);
                    if (ta->on_change) ta->on_change(ta->callback_ud);
                }
                break;
            }
            if (ctrl && key == 'd') { textarea_delete_line(ta); break; }
            if (ctrl && key == 'l') { textarea_duplicate_line(ta); break; }

            switch (key) {
                case 0xFF51: move_cursor(ta,  0, -1, shift); break; /* left  */
                case 0xFF53: move_cursor(ta,  0,  1, shift); break; /* right */
                case 0xFF52: move_cursor(ta, -1,  0, shift); break; /* up    */
                case 0xFF54: move_cursor(ta,  1,  0, shift); break; /* down  */
                case 0xFF55: /* page up */
                    move_cursor(ta, -visible_rows(ta), 0, shift); break;
                case 0xFF56: /* page down */
                    move_cursor(ta,  visible_rows(ta), 0, shift); break;
                case 0xFF50: /* home */
                    if (!shift) ta->has_selection = 0;
                    else if (!ta->has_selection) ta->mark = ta->cursor;
                    ta->cursor.col = 0;
                    if (shift) ta->has_selection = (ta->cursor.col != ta->mark.col || ta->cursor.row != ta->mark.row);
                    textarea_scroll_to_cursor(ta);
                    widget_set_dirty(w);
                    break;
                case 0xFF57: /* end */
                    if (!shift) ta->has_selection = 0;
                    else if (!ta->has_selection) ta->mark = ta->cursor;
                    ta->cursor.col = ta->lines[ta->cursor.row].len;
                    if (shift) ta->has_selection = 1;
                    textarea_scroll_to_cursor(ta);
                    widget_set_dirty(w);
                    break;
                case 0xFF08: /* backspace */
                    textarea_backspace(ta); break;
                case 0xFFFF: /* delete */
                    textarea_delete_fwd(ta); break;
                case '\r': case '\n':
                    textarea_newline(ta); break;
                case '\t':
                    if (shift) textarea_dedent(ta);
                    else       textarea_indent(ta);
                    break;
                default:
                    if (ev->key.cp >= 32 && ev->key.cp < 65536 && !ctrl)
                        textarea_insert_char(ta, ev->key.cp);
                    break;
            }
            break;
        }

        case WIDGET_EV_TIMER:
            ta->blink_timer++;
            if ((ta->blink_timer % CURSOR_BLINK) == 0 && focused)
                widget_set_dirty(w);
            break;

        default:
            break;
    }
}