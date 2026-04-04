#include "editor.h"
#include "buffer.h"
#include <stdlib.h>
#include <string.h>

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = xmalloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/* -----------------------------------------------------------------------
 * Editor struct
 * --------------------------------------------------------------------- */

struct Editor {
    Buffer *buf;

    int     tab_width;
    int     use_spaces;
    int     auto_indent;
    int     show_line_numbers;
    int     word_wrap;
    int     wrap_col;

    char   *clipboard;
    size_t  clipboard_len;
};

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

Editor *editor_new(Buffer *buf) {
    if (!buf) return NULL;
    Editor *e = xmalloc(sizeof(Editor));
    if (!e) return NULL;
    memset(e, 0, sizeof(Editor));

    e->buf              = buf;
    e->tab_width        = 4;
    e->use_spaces       = 1;
    e->auto_indent      = 1;
    e->show_line_numbers= 1;
    e->word_wrap        = 0;
    e->wrap_col         = 80;

    return e;
}

void editor_free(Editor *e) {
    if (!e) return;
    free(e->clipboard);
    free(e);
}

/* -----------------------------------------------------------------------
 * Settings
 * --------------------------------------------------------------------- */

void editor_set_tab_width(Editor *e, int w) {
    if (!e || w < 1 || w > 16) return;
    e->tab_width = w;
}

void editor_set_use_spaces(Editor *e, int use_spaces) {
    if (!e) return;
    e->use_spaces = use_spaces;
}

void editor_set_auto_indent(Editor *e, int on) {
    if (!e) return;
    e->auto_indent = on;
}

void editor_set_word_wrap(Editor *e, int on, int col) {
    if (!e) return;
    e->word_wrap = on;
    e->wrap_col  = col > 0 ? col : 80;
}

int editor_tab_width(Editor *e)         { return e ? e->tab_width : 4; }
int editor_use_spaces(Editor *e)        { return e ? e->use_spaces : 1; }
int editor_auto_indent(Editor *e)       { return e ? e->auto_indent : 1; }
int editor_show_line_numbers(Editor *e) { return e ? e->show_line_numbers : 1; }

/* -----------------------------------------------------------------------
 * Newline with auto-indent
 * --------------------------------------------------------------------- */

void editor_newline(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    size_t pos = buffer_cursor(b);

    if (buffer_has_selection(b)) {
        buffer_delete_selection(b);
        pos = buffer_cursor(b);
    }

    int line, col;
    buffer_pos_to_lc(b, pos, &line, &col);
    (void)col;

    int indent = 0;
    if (e->auto_indent) {
        char *line_text = buffer_line_text(b, line);
        if (line_text) {
            while (line_text[indent] == ' ' || line_text[indent] == '\t')
                indent++;
            free(line_text);
        }
        char prev = pos > 0 ? buffer_char_at(b, pos - 1) : '\0';
        if (prev == '{' || prev == ':' || prev == '(')
            indent += e->tab_width;
    }

    buffer_insert_char(b, pos, '\n');
    pos++;

    if (indent > 0) {
        char *spaces = xmalloc(indent + 1);
        if (spaces) {
            memset(spaces, ' ', indent);
            spaces[indent] = '\0';
            buffer_insert(b, pos, spaces, indent);
            pos += indent;
            free(spaces);
        }
    }

    buffer_cursor_set(b, pos);
}

/* -----------------------------------------------------------------------
 * Tab insertion
 * --------------------------------------------------------------------- */

void editor_insert_tab(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    if (buffer_has_selection(b)) {
        editor_indent_selection(e);
        return;
    }

    size_t pos = buffer_cursor(b);

    if (e->use_spaces) {
        int line, col;
        buffer_pos_to_lc(b, pos, &line, &col);
        (void)line;
        int spaces = e->tab_width - (col % e->tab_width);
        char *buf = xmalloc(spaces + 1);
        if (buf) {
            memset(buf, ' ', spaces);
            buf[spaces] = '\0';
            buffer_insert(b, pos, buf, spaces);
            buffer_cursor_move(b, spaces);
            free(buf);
        }
    } else {
        buffer_insert_char(b, pos, '\t');
        buffer_cursor_move(b, 1);
    }
}

/* -----------------------------------------------------------------------
 * Backspace with smart dedent
 * --------------------------------------------------------------------- */

void editor_backspace(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    if (buffer_has_selection(b)) {
        buffer_delete_selection(b);
        return;
    }

    size_t pos = buffer_cursor(b);
    if (pos == 0) return;

    if (e->use_spaces) {
        int line, col;
        buffer_pos_to_lc(b, pos, &line, &col);
        (void)line;

        if (col > 0 && col % e->tab_width == 0) {
            int all_spaces = 1;
            size_t line_start = buffer_line_start(b, line);
            for (size_t i = line_start; i < pos; i++) {
                if (buffer_char_at(b, i) != ' ') { all_spaces = 0; break; }
            }
            if (all_spaces && col >= e->tab_width) {
                buffer_delete(b, pos - e->tab_width, e->tab_width);
                buffer_cursor_set(b, pos - e->tab_width);
                return;
            }
        }
    }

    buffer_delete_char_before(b, pos);
    buffer_cursor_set(b, pos - 1);
}

/* -----------------------------------------------------------------------
 * Indent / dedent selection
 * --------------------------------------------------------------------- */

void editor_indent_selection(Editor *e) {
    if (!e || !buffer_has_selection(e->buf)) return;
    Buffer *b = e->buf;

    size_t sel_start, sel_end;
    buffer_selection_range(b, &sel_start, &sel_end);

    int first_line, last_line, dummy;
    buffer_pos_to_lc(b, sel_start, &first_line, &dummy);
    buffer_pos_to_lc(b, sel_end,   &last_line,  &dummy);

    char *indent_str;
    int   indent_len;
    if (e->use_spaces) {
        indent_str = xmalloc(e->tab_width + 1);
        if (!indent_str) return;
        memset(indent_str, ' ', e->tab_width);
        indent_str[e->tab_width] = '\0';
        indent_len = e->tab_width;
    } else {
        indent_str = xstrdup("\t");
        if (!indent_str) return;
        indent_len = 1;
    }

    for (int ln = first_line; ln <= last_line; ln++) {
        size_t ls = buffer_line_start(b, ln);
        buffer_insert(b, ls, indent_str, indent_len);
    }

    free(indent_str);
    buffer_mark_clear(b);
}

void editor_dedent_selection(Editor *e) {
    if (!e || !buffer_has_selection(e->buf)) return;
    Buffer *b = e->buf;

    size_t sel_start, sel_end;
    buffer_selection_range(b, &sel_start, &sel_end);

    int first_line, last_line, dummy;
    buffer_pos_to_lc(b, sel_start, &first_line, &dummy);
    buffer_pos_to_lc(b, sel_end,   &last_line,  &dummy);

    for (int ln = first_line; ln <= last_line; ln++) {
        size_t ls  = buffer_line_start(b, ln);
        char   c   = buffer_char_at(b, ls);
        if (c == '\t') {
            buffer_delete(b, ls, 1);
        } else if (c == ' ') {
            int removed = 0;
            while (removed < e->tab_width &&
                   buffer_char_at(b, ls) == ' ') {
                buffer_delete(b, ls, 1);
                removed++;
            }
        }
    }

    buffer_mark_clear(b);
}

/* -----------------------------------------------------------------------
 * Duplicate line
 * --------------------------------------------------------------------- */

void editor_duplicate_line(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    size_t pos = buffer_cursor(b);
    int line, col;
    buffer_pos_to_lc(b, pos, &line, &col);
    (void)col;

    char *text = buffer_line_text(b, line);
    if (!text) return;

    size_t line_end_pos = buffer_line_end(b, line);
    size_t len = strlen(text);

    buffer_insert_char(b, line_end_pos, '\n');
    buffer_insert(b, line_end_pos + 1, text, len);
    buffer_cursor_set(b, line_end_pos + 1 + (pos - buffer_line_start(b, line)));

    free(text);
}

/* -----------------------------------------------------------------------
 * Delete line
 * --------------------------------------------------------------------- */

void editor_delete_line(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    size_t pos = buffer_cursor(b);
    int line, col;
    buffer_pos_to_lc(b, pos, &line, &col);
    (void)col;

    size_t ls = buffer_line_start(b, line);
    int total_lines = buffer_line_count(b);
    size_t le;

    if (line + 1 < total_lines)
        le = buffer_line_start(b, line + 1);
    else
        le = buffer_length(b);

    buffer_delete(b, ls, le - ls);

    if (ls >= buffer_length(b) && ls > 0)
        buffer_cursor_set(b, buffer_length(b));
    else
        buffer_cursor_set(b, ls);
}

/* -----------------------------------------------------------------------
 * Move line up / down
 * --------------------------------------------------------------------- */

void editor_move_line_up(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    size_t pos = buffer_cursor(b);
    int line, col;
    buffer_pos_to_lc(b, pos, &line, &col);

    if (line == 0) return;

    char *cur  = buffer_line_text(b, line);
    char *prev = buffer_line_text(b, line - 1);
    if (!cur || !prev) { free(cur); free(prev); return; }

    size_t cur_len  = strlen(cur);
    size_t prev_len = strlen(prev);
    size_t prev_start = buffer_line_start(b, line - 1);

    buffer_delete(b, prev_start, prev_len + 1 + cur_len);

    buffer_insert(b, prev_start, cur, cur_len);
    buffer_insert_char(b, prev_start + cur_len, '\n');
    buffer_insert(b, prev_start + cur_len + 1, prev, prev_len);

    buffer_cursor_set(b, buffer_lc_to_pos(b, line - 1, col));

    free(cur);
    free(prev);
}

void editor_move_line_down(Editor *e) {
    if (!e) return;
    Buffer *b = e->buf;

    size_t pos = buffer_cursor(b);
    int line, col;
    buffer_pos_to_lc(b, pos, &line, &col);

    if (line + 1 >= buffer_line_count(b)) return;

    char *cur  = buffer_line_text(b, line);
    char *next = buffer_line_text(b, line + 1);
    if (!cur || !next) { free(cur); free(next); return; }

    size_t cur_len  = strlen(cur);
    size_t next_len = strlen(next);
    size_t cur_start = buffer_line_start(b, line);

    buffer_delete(b, cur_start, cur_len + 1 + next_len);

    buffer_insert(b, cur_start, next, next_len);
    buffer_insert_char(b, cur_start + next_len, '\n');
    buffer_insert(b, cur_start + next_len + 1, cur, cur_len);

    buffer_cursor_set(b, buffer_lc_to_pos(b, line + 1, col));

    free(cur);
    free(next);
}

/* -----------------------------------------------------------------------
 * Toggle line comment
 * --------------------------------------------------------------------- */

void editor_toggle_comment(Editor *e, const char *comment_prefix) {
    if (!e || !comment_prefix) return;
    Buffer *b = e->buf;
    size_t prefix_len = strlen(comment_prefix);

    int first_line, last_line, dummy;
    if (buffer_has_selection(b)) {
        size_t sel_start, sel_end;
        buffer_selection_range(b, &sel_start, &sel_end);
        buffer_pos_to_lc(b, sel_start, &first_line, &dummy);
        buffer_pos_to_lc(b, sel_end,   &last_line,  &dummy);
    } else {
        buffer_pos_to_lc(b, buffer_cursor(b), &first_line, &dummy);
        last_line = first_line;
    }

    int all_commented = 1;
    for (int ln = first_line; ln <= last_line; ln++) {
        char *text = buffer_line_text(b, ln);
        if (!text) { all_commented = 0; break; }
        int i = 0;
        while (text[i] == ' ' || text[i] == '\t') i++;
        if (strncmp(text + i, comment_prefix, prefix_len) != 0)
            all_commented = 0;
        free(text);
        if (!all_commented) break;
    }

    for (int ln = first_line; ln <= last_line; ln++) {
        size_t ls   = buffer_line_start(b, ln);
        char *text  = buffer_line_text(b, ln);
        if (!text) continue;

        if (all_commented) {
            int i = 0;
            while (text[i] == ' ' || text[i] == '\t') i++;
            buffer_delete(b, ls + i, prefix_len);
        } else {
            buffer_insert(b, ls, comment_prefix, prefix_len);
        }
        free(text);
    }

    buffer_mark_clear(b);
}

/* -----------------------------------------------------------------------
 * Internal clipboard (fallback if no system clipboard)
 * --------------------------------------------------------------------- */

void editor_clipboard_copy(Editor *e) {
    if (!e || !buffer_has_selection(e->buf)) return;
    char *text;
    size_t len;
    if (buffer_copy(e->buf, &text, &len) == 0) {
        free(e->clipboard);
        e->clipboard     = text;
        e->clipboard_len = len;
    }
}

void editor_clipboard_cut(Editor *e) {
    if (!e || !buffer_has_selection(e->buf)) return;
    char *text;
    size_t len;
    if (buffer_cut(e->buf, &text, &len) == 0) {
        free(e->clipboard);
        e->clipboard     = text;
        e->clipboard_len = len;
    }
}

void editor_clipboard_paste(Editor *e) {
    if (!e || !e->clipboard || e->clipboard_len == 0) return;
    buffer_paste(e->buf, e->clipboard, e->clipboard_len);
}

/* -----------------------------------------------------------------------
 * Go to line
 * --------------------------------------------------------------------- */

void editor_goto_line(Editor *e, int line) {
    if (!e) return;
    buffer_cursor_set(e->buf, buffer_lc_to_pos(e->buf, line - 1, 0));
}

/* -----------------------------------------------------------------------
 * Word under cursor
 * --------------------------------------------------------------------- */

char *editor_word_at_cursor(Editor *e) {
    if (!e) return NULL;
    Buffer *b   = e->buf;
    size_t  pos = buffer_cursor(b);
    size_t  len = buffer_length(b);

    size_t start = pos;
    while (start > 0) {
        char c = buffer_char_at(b, start - 1);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) break;
        start--;
    }

    size_t end = pos;
    while (end < len) {
        char c = buffer_char_at(b, end);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) break;
        end++;
    }

    if (end <= start) return NULL;
    size_t word_len = end - start;
    char *word = xmalloc(word_len + 1);
    if (!word) return NULL;
    for (size_t i = 0; i < word_len; i++)
        word[i] = buffer_char_at(b, start + i);
    word[word_len] = '\0';
    return word;
}

/* -----------------------------------------------------------------------
 * Select word at cursor
 * --------------------------------------------------------------------- */

void editor_select_word(Editor *e) {
    if (!e) return;
    Buffer *b   = e->buf;
    size_t  pos = buffer_cursor(b);
    size_t  len = buffer_length(b);

    size_t start = pos;
    while (start > 0) {
        char c = buffer_char_at(b, start - 1);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) break;
        start--;
    }

    size_t end = pos;
    while (end < len) {
        char c = buffer_char_at(b, end);
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) break;
        end++;
    }

    if (end > start) {
        buffer_mark_set(b, start);
        buffer_cursor_set(b, end);
    }
}

/* -----------------------------------------------------------------------
 * Accessors
 * --------------------------------------------------------------------- */

Buffer *editor_buffer(Editor *e) {
    return e ? e->buf : NULL;
}