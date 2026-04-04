#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include <stddef.h>

typedef struct Editor Editor;

Editor *editor_new(Buffer *buf);
void editor_free(Editor *e);

void editor_set_tab_width(Editor *e, int w);
void editor_set_use_spaces(Editor *e, int use_spaces);
void editor_set_auto_indent(Editor *e, int on);
void editor_set_word_wrap(Editor *e, int on, int col);
int editor_tab_width(Editor *e);
int editor_use_spaces(Editor *e);
int editor_auto_indent(Editor *e);
int editor_show_line_numbers(Editor *e);

void editor_newline(Editor *e);
void editor_insert_tab(Editor *e);
void editor_backspace(Editor *e);

void editor_indent_selection(Editor *e);
void editor_dedent_selection(Editor *e);

void editor_duplicate_line(Editor *e);
void editor_delete_line(Editor *e);
void editor_move_line_up(Editor *e);
void editor_move_line_down(Editor *e);

void editor_toggle_comment(Editor *e, const char *comment_prefix);

void editor_clipboard_copy(Editor *e);
void editor_clipboard_cut(Editor *e);
void editor_clipboard_paste(Editor *e);

void editor_goto_line(Editor *e, int line);
char *editor_word_at_cursor(Editor *e);
void editor_select_word(Editor *e);

Buffer *editor_buffer(Editor *e);

#endif