#include "buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GAP_INITIAL     4096
#define GAP_GROW        4096
#define UNDO_CAP        256
#define LINE_CACHE_CAP  8192

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) return NULL;
    return q;
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
 * UndoRecord
 * --------------------------------------------------------------------- */

typedef enum {
    UNDO_INSERT,
    UNDO_DELETE,
} UndoType;

typedef struct {
    UndoType type;
    size_t   pos;
    char    *text;
    size_t   len;
} UndoRecord;

/* -----------------------------------------------------------------------
 * Buffer struct
 * --------------------------------------------------------------------- */

struct Buffer {
    char   *data;
    size_t  gap_start;
    size_t  gap_end;
    size_t  alloc;

    size_t  cursor;
    size_t  mark;
    int     mark_active;

    int     modified;
    char   *filename;

    UndoRecord *undo_stack;
    int         undo_top;
    int         undo_cap;
    int         in_undo;

    size_t *line_offsets;
    int     line_count;
    int     line_cap;
    int     lines_dirty;
};

/* -----------------------------------------------------------------------
 * Gap buffer low-level
 * --------------------------------------------------------------------- */

static size_t buf_length(Buffer *b) {
    return b->alloc - (b->gap_end - b->gap_start);
}

static void gap_move(Buffer *b, size_t pos) {
    if (pos == b->gap_start) return;

    if (pos < b->gap_start) {
        size_t move = b->gap_start - pos;
        memmove(b->data + b->gap_end - move,
                b->data + pos,
                move);
        b->gap_end   -= move;
        b->gap_start -= move;
    } else {
        size_t move = pos - b->gap_start;
        memmove(b->data + b->gap_start,
                b->data + b->gap_end,
                move);
        b->gap_start += move;
        b->gap_end   += move;
    }
}

static int gap_ensure(Buffer *b, size_t needed) {
    size_t gap_size = b->gap_end - b->gap_start;
    if (gap_size >= needed) return 0;

    size_t grow   = needed - gap_size + GAP_GROW;
    size_t new_alloc = b->alloc + grow;
    char *new_data = xrealloc(b->data, new_alloc);
    if (!new_data) return -1;

    memmove(new_data + b->gap_end + grow,
            new_data + b->gap_end,
            b->alloc - b->gap_end);

    b->data      = new_data;
    b->gap_end  += grow;
    b->alloc     = new_alloc;
    return 0;
}

/* -----------------------------------------------------------------------
 * Convert logical position to physical index
 * --------------------------------------------------------------------- */

static size_t phys(Buffer *b, size_t pos) {
    if (pos < b->gap_start) return pos;
    return pos + (b->gap_end - b->gap_start);
}

/* -----------------------------------------------------------------------
 * Line cache
 * --------------------------------------------------------------------- */

static void lines_rebuild(Buffer *b) {
    size_t len = buf_length(b);

    if (b->line_cap < LINE_CACHE_CAP) {
        size_t *nl = xrealloc(b->line_offsets,
                               sizeof(size_t) * LINE_CACHE_CAP);
        if (!nl) return;
        b->line_offsets = nl;
        b->line_cap     = LINE_CACHE_CAP;
    }

    b->line_count    = 0;
    b->line_offsets[b->line_count++] = 0;

    for (size_t i = 0; i < len; i++) {
        char c = b->data[phys(b, i)];
        if (c == '\n') {
            if (b->line_count >= b->line_cap) {
                int new_cap = b->line_cap * 2;
                size_t *nl = xrealloc(b->line_offsets,
                                       sizeof(size_t) * new_cap);
                if (!nl) break;
                b->line_offsets = nl;
                b->line_cap     = new_cap;
            }
            b->line_offsets[b->line_count++] = i + 1;
        }
    }
    b->lines_dirty = 0;
}

static void lines_mark_dirty(Buffer *b) {
    b->lines_dirty = 1;
}

static void lines_ensure(Buffer *b) {
    if (b->lines_dirty) lines_rebuild(b);
}

/* -----------------------------------------------------------------------
 * Undo stack
 * --------------------------------------------------------------------- */

static void undo_clear_from(Buffer *b, int top) {
    for (int i = top; i < b->undo_cap; i++) {
        free(b->undo_stack[i].text);
        b->undo_stack[i].text = NULL;
        b->undo_stack[i].len  = 0;
    }
}

static void undo_push(Buffer *b, UndoType type, size_t pos,
                       const char *text, size_t len) {
    if (b->in_undo) return;
    if (!b->undo_stack) {
        b->undo_stack = xmalloc(sizeof(UndoRecord) * UNDO_CAP);
        if (!b->undo_stack) return;
        memset(b->undo_stack, 0, sizeof(UndoRecord) * UNDO_CAP);
        b->undo_cap = UNDO_CAP;
        b->undo_top = 0;
    }

    undo_clear_from(b, b->undo_top);

    if (b->undo_top >= b->undo_cap) {
        int new_cap = b->undo_cap * 2;
        UndoRecord *nr = xrealloc(b->undo_stack,
                                   sizeof(UndoRecord) * new_cap);
        if (!nr) return;
        b->undo_stack = nr;
        b->undo_cap   = new_cap;
    }

    UndoRecord *r = &b->undo_stack[b->undo_top];
    r->type = type;
    r->pos  = pos;
    r->len  = len;
    r->text = xmalloc(len + 1);
    if (!r->text) return;
    memcpy(r->text, text, len);
    r->text[len] = '\0';

    b->undo_top++;
}

/* -----------------------------------------------------------------------
 * Public: lifecycle
 * --------------------------------------------------------------------- */

Buffer *buffer_new(void) {
    Buffer *b = xmalloc(sizeof(Buffer));
    if (!b) return NULL;
    memset(b, 0, sizeof(Buffer));

    b->alloc = GAP_INITIAL;
    b->data  = xmalloc(GAP_INITIAL);
    if (!b->data) { free(b); return NULL; }

    b->gap_start = 0;
    b->gap_end   = GAP_INITIAL;
    b->cursor    = 0;
    b->modified  = 0;
    b->lines_dirty = 1;

    return b;
}

void buffer_free(Buffer *b) {
    if (!b) return;
    free(b->data);
    free(b->filename);
    if (b->undo_stack) {
        undo_clear_from(b, 0);
        free(b->undo_stack);
    }
    free(b->line_offsets);
    free(b);
}

/* -----------------------------------------------------------------------
 * Public: load / save
 * --------------------------------------------------------------------- */

int buffer_load(Buffer *b, const char *path) {
    if (!b || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) { fclose(f); return -1; }
    if (file_size == 0) {
        fclose(f);
        free(b->filename);
        b->filename = xstrdup(path);
        b->modified = 0;
        b->cursor   = 0;
        lines_mark_dirty(b);
        return 0;
    }

    size_t new_alloc = (size_t)file_size + GAP_INITIAL;
    char *new_data   = xrealloc(b->data, new_alloc);
    if (!new_data) { fclose(f); return -1; }
    b->data  = new_data;
    b->alloc = new_alloc;

    size_t n = fread(b->data, 1, (size_t)file_size, f);
    fclose(f);
    if ((long)n != file_size) return -1;

    b->gap_start = (size_t)file_size;
    b->gap_end   = new_alloc;

    free(b->filename);
    b->filename = xstrdup(path);
    b->cursor   = 0;
    b->modified = 0;
    b->undo_top = 0;
    lines_mark_dirty(b);
    return 0;
}

int buffer_save(Buffer *b, const char *path) {
    if (!b) return -1;
    const char *target = path ? path : b->filename;
    if (!target) return -1;

    FILE *f = fopen(target, "wb");
    if (!f) return -1;

    size_t len = buf_length(b);
    int ok = 1;

    if (b->gap_start > 0) {
        if (fwrite(b->data, 1, b->gap_start, f) != b->gap_start) ok = 0;
    }
    if (ok) {
        size_t after = len - b->gap_start;
        if (after > 0) {
            if (fwrite(b->data + b->gap_end, 1, after, f) != after) ok = 0;
        }
    }
    fclose(f);

    if (ok) {
        if (path && path != b->filename) {
            free(b->filename);
            b->filename = xstrdup(path);
        }
        b->modified = 0;
    }
    return ok ? 0 : -1;
}

/* -----------------------------------------------------------------------
 * Public: queries
 * --------------------------------------------------------------------- */

size_t buffer_length(Buffer *b) {
    if (!b) return 0;
    return buf_length(b);
}

char buffer_char_at(Buffer *b, size_t pos) {
    if (!b || pos >= buf_length(b)) return '\0';
    return b->data[phys(b, pos)];
}

size_t buffer_cursor(Buffer *b) {
    return b ? b->cursor : 0;
}

int buffer_modified(Buffer *b) {
    return b ? b->modified : 0;
}

const char *buffer_filename(Buffer *b) {
    return b ? b->filename : NULL;
}

int buffer_line_count(Buffer *b) {
    if (!b) return 0;
    lines_ensure(b);
    return b->line_count;
}

void buffer_pos_to_lc(Buffer *b, size_t pos, int *line, int *col) {
    if (!b) { if (line) *line = 0; if (col) *col = 0; return; }
    lines_ensure(b);

    int lo = 0, hi = b->line_count - 1, found = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (b->line_offsets[mid] <= pos &&
            (mid + 1 >= b->line_count || b->line_offsets[mid + 1] > pos)) {
            found = mid;
            break;
        }
        if (b->line_offsets[mid] > pos) hi = mid - 1;
        else lo = mid + 1;
    }

    if (line) *line = found;
    if (col)  *col  = (int)(pos - b->line_offsets[found]);
}

size_t buffer_lc_to_pos(Buffer *b, int line, int col) {
    if (!b) return 0;
    lines_ensure(b);
    if (line < 0)                line = 0;
    if (line >= b->line_count)   line = b->line_count - 1;
    size_t line_start = b->line_offsets[line];
    size_t line_end   = (line + 1 < b->line_count)
                        ? b->line_offsets[line + 1] - 1
                        : buf_length(b);
    size_t line_len   = line_end - line_start;
    if ((size_t)col > line_len) col = (int)line_len;
    return line_start + col;
}

size_t buffer_line_start(Buffer *b, int line) {
    if (!b) return 0;
    lines_ensure(b);
    if (line < 0)              line = 0;
    if (line >= b->line_count) line = b->line_count - 1;
    return b->line_offsets[line];
}

size_t buffer_line_end(Buffer *b, int line) {
    if (!b) return 0;
    lines_ensure(b);
    if (line < 0)              line = 0;
    if (line >= b->line_count) line = b->line_count - 1;
    size_t next = (line + 1 < b->line_count)
                  ? b->line_offsets[line + 1]
                  : buf_length(b);
    if (next > 0 && buffer_char_at(b, next - 1) == '\n') next--;
    return next;
}

int buffer_line_length(Buffer *b, int line) {
    if (!b) return 0;
    return (int)(buffer_line_end(b, line) - buffer_line_start(b, line));
}

char *buffer_line_text(Buffer *b, int line) {
    if (!b) return NULL;
    size_t start = buffer_line_start(b, line);
    size_t end   = buffer_line_end(b, line);
    size_t len   = end - start;
    char *out = xmalloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = buffer_char_at(b, start + i);
    out[len] = '\0';
    return out;
}

/* -----------------------------------------------------------------------
 * Public: cursor movement
 * --------------------------------------------------------------------- */

void buffer_cursor_set(Buffer *b, size_t pos) {
    if (!b) return;
    size_t len = buf_length(b);
    if (pos > len) pos = len;
    b->cursor = pos;
}

void buffer_cursor_move(Buffer *b, int delta) {
    if (!b) return;
    size_t len = buf_length(b);
    if (delta < 0) {
        size_t d = (size_t)(-delta);
        b->cursor = (b->cursor >= d) ? b->cursor - d : 0;
    } else {
        b->cursor += delta;
        if (b->cursor > len) b->cursor = len;
    }
}

void buffer_cursor_line(Buffer *b, int delta) {
    if (!b) return;
    int line, col;
    buffer_pos_to_lc(b, b->cursor, &line, &col);
    line += delta;
    b->cursor = buffer_lc_to_pos(b, line, col);
}

void buffer_cursor_home(Buffer *b) {
    if (!b) return;
    int line, col;
    buffer_pos_to_lc(b, b->cursor, &line, &col);
    (void)col;
    b->cursor = buffer_line_start(b, line);
}

void buffer_cursor_end(Buffer *b) {
    if (!b) return;
    int line, col;
    buffer_pos_to_lc(b, b->cursor, &line, &col);
    (void)col;
    b->cursor = buffer_line_end(b, line);
}

void buffer_cursor_word_forward(Buffer *b) {
    if (!b) return;
    size_t len = buf_length(b);
    size_t pos = b->cursor;
    while (pos < len && buffer_char_at(b, pos) == ' ') pos++;
    while (pos < len && buffer_char_at(b, pos) != ' ' &&
           buffer_char_at(b, pos) != '\n') pos++;
    b->cursor = pos;
}

void buffer_cursor_word_back(Buffer *b) {
    if (!b) return;
    size_t pos = b->cursor;
    if (pos == 0) return;
    pos--;
    while (pos > 0 && buffer_char_at(b, pos) == ' ') pos--;
    while (pos > 0 && buffer_char_at(b, pos - 1) != ' ' &&
           buffer_char_at(b, pos - 1) != '\n') pos--;
    b->cursor = pos;
}

/* -----------------------------------------------------------------------
 * Public: insert / delete
 * --------------------------------------------------------------------- */

int buffer_insert(Buffer *b, size_t pos, const char *text, size_t len) {
    if (!b || !text || len == 0) return -1;
    if (pos > buf_length(b)) pos = buf_length(b);

    if (gap_ensure(b, len) < 0) return -1;
    gap_move(b, pos);

    memcpy(b->data + b->gap_start, text, len);
    b->gap_start += len;

    if (b->cursor >= pos) b->cursor += len;
    b->modified = 1;
    lines_mark_dirty(b);

    undo_push(b, UNDO_INSERT, pos, text, len);
    return 0;
}

int buffer_insert_char(Buffer *b, size_t pos, char c) {
    return buffer_insert(b, pos, &c, 1);
}

int buffer_delete(Buffer *b, size_t pos, size_t len) {
    if (!b || len == 0) return -1;
    size_t buf_len = buf_length(b);
    if (pos >= buf_len) return -1;
    if (pos + len > buf_len) len = buf_len - pos;

    char *deleted = xmalloc(len + 1);
    if (!deleted) return -1;
    for (size_t i = 0; i < len; i++)
        deleted[i] = buffer_char_at(b, pos + i);
    deleted[len] = '\0';

    undo_push(b, UNDO_DELETE, pos, deleted, len);
    free(deleted);

    gap_move(b, pos);
    b->gap_end += len;

    if (b->cursor > pos) {
        if (b->cursor <= pos + len) b->cursor = pos;
        else b->cursor -= len;
    }
    b->modified = 1;
    lines_mark_dirty(b);
    return 0;
}

int buffer_delete_range(Buffer *b, size_t start, size_t end) {
    if (!b || end <= start) return -1;
    return buffer_delete(b, start, end - start);
}

int buffer_delete_char_before(Buffer *b, size_t pos) {
    if (!b || pos == 0) return -1;
    return buffer_delete(b, pos - 1, 1);
}

int buffer_delete_char_after(Buffer *b, size_t pos) {
    return buffer_delete(b, pos, 1);
}

int buffer_delete_line(Buffer *b, int line) {
    if (!b) return -1;
    lines_ensure(b);
    if (line < 0 || line >= b->line_count) return -1;
    size_t start = buffer_line_start(b, line);
    size_t end   = (line + 1 < b->line_count)
                   ? b->line_offsets[line + 1]
                   : buf_length(b);
    return buffer_delete(b, start, end - start);
}

/* -----------------------------------------------------------------------
 * Public: selection
 * --------------------------------------------------------------------- */

void buffer_mark_set(Buffer *b, size_t pos) {
    if (!b) return;
    b->mark        = pos;
    b->mark_active = 1;
}

void buffer_mark_clear(Buffer *b) {
    if (!b) return;
    b->mark_active = 0;
}

int buffer_has_selection(Buffer *b) {
    return b && b->mark_active && b->mark != b->cursor;
}

void buffer_selection_range(Buffer *b, size_t *start, size_t *end) {
    if (!b || !start || !end) return;
    if (b->cursor < b->mark) {
        *start = b->cursor;
        *end   = b->mark;
    } else {
        *start = b->mark;
        *end   = b->cursor;
    }
}

char *buffer_selection_text(Buffer *b) {
    if (!buffer_has_selection(b)) return NULL;
    size_t start, end;
    buffer_selection_range(b, &start, &end);
    size_t len = end - start;
    char *out  = xmalloc(len + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; i++)
        out[i] = buffer_char_at(b, start + i);
    out[len] = '\0';
    return out;
}

int buffer_delete_selection(Buffer *b) {
    if (!buffer_has_selection(b)) return -1;
    size_t start, end;
    buffer_selection_range(b, &start, &end);
    buffer_mark_clear(b);
    b->cursor = start;
    return buffer_delete(b, start, end - start);
}

/* -----------------------------------------------------------------------
 * Public: clipboard helpers
 * --------------------------------------------------------------------- */

int buffer_copy(Buffer *b, char **out_text, size_t *out_len) {
    if (!b || !out_text) return -1;
    char *text = buffer_selection_text(b);
    if (!text) return -1;
    *out_text = text;
    if (out_len) *out_len = strlen(text);
    return 0;
}

int buffer_cut(Buffer *b, char **out_text, size_t *out_len) {
    if (buffer_copy(b, out_text, out_len) < 0) return -1;
    buffer_delete_selection(b);
    return 0;
}

int buffer_paste(Buffer *b, const char *text, size_t len) {
    if (!b || !text || len == 0) return -1;
    if (buffer_has_selection(b)) buffer_delete_selection(b);
    int r = buffer_insert(b, b->cursor, text, len);
    if (r == 0) b->cursor += len;
    return r;
}

/* -----------------------------------------------------------------------
 * Public: undo / redo
 * --------------------------------------------------------------------- */

int buffer_undo(Buffer *b) {
    if (!b || b->undo_top == 0) return -1;
    UndoRecord *r = &b->undo_stack[b->undo_top - 1];
    b->in_undo = 1;

    if (r->type == UNDO_INSERT) {
        buffer_delete(b, r->pos, r->len);
        b->cursor = r->pos;
    } else {
        buffer_insert(b, r->pos, r->text, r->len);
        b->cursor = r->pos + r->len;
    }

    b->in_undo = 0;
    b->undo_top--;
    return 0;
}

int buffer_redo(Buffer *b) {
    if (!b) return -1;
    if (b->undo_top >= b->undo_cap) return -1;
    UndoRecord *r = &b->undo_stack[b->undo_top];
    if (!r->text) return -1;

    b->in_undo = 1;
    if (r->type == UNDO_INSERT) {
        buffer_insert(b, r->pos, r->text, r->len);
        b->cursor = r->pos + r->len;
    } else {
        buffer_delete(b, r->pos, r->len);
        b->cursor = r->pos;
    }
    b->in_undo = 0;
    b->undo_top++;
    return 0;
}

/* -----------------------------------------------------------------------
 * Public: search
 * --------------------------------------------------------------------- */

size_t buffer_find(Buffer *b, size_t from, const char *needle,
                   size_t needle_len, int case_sensitive) {
    if (!b || !needle || needle_len == 0) return BUFFER_NPOS;
    size_t len = buf_length(b);
    if (from + needle_len > len) return BUFFER_NPOS;

    for (size_t i = from; i + needle_len <= len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            char a = buffer_char_at(b, i + j);
            char c = needle[j];
            if (!case_sensitive) {
                if (a >= 'A' && a <= 'Z') a += 32;
                if (c >= 'A' && c <= 'Z') c += 32;
            }
            if (a != c) { match = 0; break; }
        }
        if (match) return i;
    }
    return BUFFER_NPOS;
}

size_t buffer_find_prev(Buffer *b, size_t from, const char *needle,
                        size_t needle_len, int case_sensitive) {
    if (!b || !needle || needle_len == 0) return BUFFER_NPOS;
    if (from < needle_len) return BUFFER_NPOS;

    size_t i = from - needle_len;
    while (1) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            char a = buffer_char_at(b, i + j);
            char c = needle[j];
            if (!case_sensitive) {
                if (a >= 'A' && a <= 'Z') a += 32;
                if (c >= 'A' && c <= 'Z') c += 32;
            }
            if (a != c) { match = 0; break; }
        }
        if (match) return i;
        if (i == 0) break;
        i--;
    }
    return BUFFER_NPOS;
}

int buffer_replace_all(Buffer *b, const char *needle, size_t needle_len,
                        const char *replacement, size_t repl_len,
                        int case_sensitive) {
    if (!b || !needle || needle_len == 0) return 0;
    int count = 0;
    size_t pos = 0;

    while (1) {
        size_t found = buffer_find(b, pos, needle, needle_len, case_sensitive);
        if (found == BUFFER_NPOS) break;
        buffer_delete(b, found, needle_len);
        buffer_insert(b, found, replacement, repl_len);
        pos = found + repl_len;
        count++;
    }
    return count;
}