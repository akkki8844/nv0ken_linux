#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include "syntax.h"
#include <stddef.h>

typedef struct Editor Editor;

typedef enum {
    MODE_NORMAL,
} EditorMode;

typedef enum {
    ACTION_NONE,
    ACTION_INSERT,
    ACTION_DELETE,
    ACTION_MOVE,
    ACTION_INDENT,
    ACTION_COPY,
    ACTION_CUT,
    ACTION_UNDO,
    ACTION_REDO,
} EditorAction;

typedef struct {
    int tab_width;
    int use_spaces;
    int auto_indent;
    int auto_pair;
    int show_line_numbers;
    int word_wrap;
} EditorConfig;

typedef struct {
    int char_count;
    int line_count;
    int word_count;
    int cursor_line;
    int cursor_col;
} EditorStats;

Editor *editor_new(Buffer *buffer);
void editor_free(Editor *editor);
void editor_set_config(Editor *editor, const EditorConfig *config);
EditorConfig editor_get_config(Editor *editor);
EditorMode editor_mode(Editor *editor);
EditorAction editor_type_char(Editor *editor, char character);
EditorAction editor_key_return(Editor *editor);
EditorAction editor_key_backspace(Editor *editor);
EditorAction editor_key_tab(Editor *editor, int shift);
EditorAction editor_key_delete(Editor *editor);
EditorAction editor_move_left(Editor *editor, int extend, int by_word);
EditorAction editor_move_right(Editor *editor, int extend, int by_word);
EditorAction editor_move_up(Editor *editor, int extend, int lines);
EditorAction editor_move_down(Editor *editor, int extend, int lines);
EditorAction editor_move_home(Editor *editor, int extend);
EditorAction editor_move_end(Editor *editor, int extend);
EditorAction editor_move_doc_start(Editor *editor, int extend);
EditorAction editor_move_doc_end(Editor *editor, int extend);
EditorAction editor_move_to_line(Editor *editor, int line);
EditorAction editor_select_all(Editor *editor);
EditorAction editor_select_word(Editor *editor);
EditorAction editor_select_line(Editor *editor);
EditorAction editor_duplicate_line(Editor *editor);
EditorAction editor_delete_line(Editor *editor);
EditorAction editor_move_line_up(Editor *editor);
EditorAction editor_move_line_down(Editor *editor);
EditorAction editor_copy(Editor *editor);
EditorAction editor_cut(Editor *editor);
EditorAction editor_paste(Editor *editor);
EditorAction editor_paste_text(Editor *editor, const char *text, size_t length);
EditorAction editor_undo(Editor *editor);
EditorAction editor_redo(Editor *editor);
EditorAction editor_toggle_comment(Editor *editor, SyntaxLang language);
size_t editor_find_matching_bracket(Editor *editor);
EditorStats editor_stats(Editor *editor);

#endif
