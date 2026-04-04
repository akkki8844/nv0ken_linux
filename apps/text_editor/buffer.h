#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>

#define BUFFER_NPOS ((size_t)-1)

typedef struct Buffer Buffer;

Buffer     *buffer_new(void);
void        buffer_free(Buffer *b);

int         buffer_load(Buffer *b, const char *path);
int         buffer_save(Buffer *b, const char *path);

size_t      buffer_length(Buffer *b);
char        buffer_char_at(Buffer *b, size_t pos);
size_t      buffer_cursor(Buffer *b);
int         buffer_modified(Buffer *b);
const char *buffer_filename(Buffer *b);

int         buffer_line_count(Buffer *b);
void        buffer_pos_to_lc(Buffer *b, size_t pos, int *line, int *col);
size_t      buffer_lc_to_pos(Buffer *b, int line, int col);
size_t      buffer_line_start(Buffer *b, int line);
size_t      buffer_line_end(Buffer *b, int line);
int         buffer_line_length(Buffer *b, int line);
char       *buffer_line_text(Buffer *b, int line);

void        buffer_cursor_set(Buffer *b, size_t pos);
void        buffer_cursor_move(Buffer *b, int delta);
void        buffer_cursor_line(Buffer *b, int delta);
void        buffer_cursor_home(Buffer *b);
void        buffer_cursor_end(Buffer *b);
void        buffer_cursor_word_forward(Buffer *b);
void        buffer_cursor_word_back(Buffer *b);

int         buffer_insert(Buffer *b, size_t pos, const char *text, size_t len);
int         buffer_insert_char(Buffer *b, size_t pos, char c);
int         buffer_delete(Buffer *b, size_t pos, size_t len);
int         buffer_delete_range(Buffer *b, size_t start, size_t end);
int         buffer_delete_char_before(Buffer *b, size_t pos);
int         buffer_delete_char_after(Buffer *b, size_t pos);
int         buffer_delete_line(Buffer *b, int line);

void        buffer_mark_set(Buffer *b, size_t pos);
void        buffer_mark_clear(Buffer *b);
int         buffer_has_selection(Buffer *b);
void        buffer_selection_range(Buffer *b, size_t *start, size_t *end);
char       *buffer_selection_text(Buffer *b);
int         buffer_delete_selection(Buffer *b);

int         buffer_copy(Buffer *b, char **out_text, size_t *out_len);
int         buffer_cut(Buffer *b, char **out_text, size_t *out_len);
int         buffer_paste(Buffer *b, const char *text, size_t len);

int         buffer_undo(Buffer *b);
int         buffer_redo(Buffer *b);

size_t      buffer_find(Buffer *b, size_t from, const char *needle,
                        size_t needle_len, int case_sensitive);
size_t      buffer_find_prev(Buffer *b, size_t from, const char *needle,
                             size_t needle_len, int case_sensitive);
int         buffer_replace_all(Buffer *b, const char *needle, size_t needle_len,
                               const char *replacement, size_t repl_len,
                               int case_sensitive);

#endif