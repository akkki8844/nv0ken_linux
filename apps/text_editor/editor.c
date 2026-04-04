#include "editor.h"
#include "buffer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define INDENT_SIZE 4
#define MAX_PAIRS 8

static void *xmalloc(size_t n) {
  void *p = malloc(n);
  if (!p)
    return NULL;
  return p;
}

static char *xstrdup(const char *s) {
  if (!s)
    return NULL;
  size_t len = strlen(s);
  char *out = xmalloc(len + 1);
  if (!out)
    return NULL;
  memcpy(out, s, len + 1);
  return out;
}

/* -----------------------------------------------------------------------
 * Auto-pair table
 * --------------------------------------------------------------------- */

static const struct {
  char open;
  char close;
} PAIRS[MAX_PAIRS] = {
    {'(', ')'},   {'[', ']'}, {'{', '}'}, {'"', '"'},
    {'\'', '\''}, {'`', '`'}, {0, 0},
};

static char pair_close(char open) {
  for (int i = 0; PAIRS[i].open; i++)
    if (PAIRS[i].open == open)
      return PAIRS[i].close;
  return 0;
}

static int is_open(char c) { return pair_close(c) != 0; }

static int is_close(char c) {
  for (int i = 0; PAIRS[i].open; i++)
    if (PAIRS[i].close == c && PAIRS[i].open != PAIRS[i].close)
      return 1;
  return 0;
}

/* -----------------------------------------------------------------------
 * Editor struct
 * --------------------------------------------------------------------- */

struct Editor {
  Buffer *buf;
  EditorMode mode;

  int auto_indent;
  int auto_pair;
  int tab_width;
  int use_spaces;

  char *clipboard;
  size_t clipboard_len;

  EditorConfig cfg;
};

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

Editor *editor_new(Buffer *buf) {
  if (!buf)
    return NULL;
  Editor *e = xmalloc(sizeof(Editor));
  if (!e)
    return NULL;
  memset(e, 0, sizeof(Editor));

  e->buf = buf;
  e->mode = MODE_NORMAL;
  e->auto_indent = 1;
  e->auto_pair = 1;
  e->tab_width = INDENT_SIZE;
  e->use_spaces = 1;

  e->cfg.tab_width = INDENT_SIZE;
  e->cfg.use_spaces = 1;
  e->cfg.auto_indent = 1;
  e->cfg.auto_pair = 1;
  e->cfg.show_line_numbers = 1;
  e->cfg.word_wrap = 0;

  return e;
}

void editor_free(Editor *e) {
  if (!e)
    return;
  free(e->clipboard);
  free(e);
}

void editor_set_config(Editor *e, const EditorConfig *cfg) {
  if (!e || !cfg)
    return;
  e->cfg = *cfg;
  e->tab_width = cfg->tab_width;
  e->use_spaces = cfg->use_spaces;
  e->auto_indent = cfg->auto_indent;
  e->auto_pair = cfg->auto_pair;
}

EditorConfig editor_get_config(Editor *e) {
  EditorConfig cfg = {0};
  if (!e)
    return cfg;
  return e->cfg;
}

EditorMode editor_mode(Editor *e) { return e ? e->mode : MODE_NORMAL; }

/* -----------------------------------------------------------------------
 * Auto-indent helpers
 * --------------------------------------------------------------------- */

static int line_indent_level(Buffer *b, int line) {
  size_t start = buffer_line_start(b, line);
  size_t end = buffer_line_end(b, line);
  int indent = 0;
  for (size_t i = start; i < end; i++) {
    char c = buffer_char_at(b, i);
    if (c == ' ') {
      indent++;
    } else if (c == '\t') {
      indent += 4;
    } else
      break;
  }
  return indent;
}

static char line_last_nonspace(Buffer *b, int line) {
  size_t start = buffer_line_start(b, line);
  size_t end = buffer_line_end(b, line);
  if (end == start)
    return '\0';
  for (size_t i = end - 1; i >= start; i--) {
    char c = buffer_char_at(b, i);
    if (c != ' ' && c != '\t')
      return c;
    if (i == start)
      break;
  }
  return '\0';
}

static char line_first_nonspace(Buffer *b, int line) {
  size_t start = buffer_line_start(b, line);
  size_t end = buffer_line_end(b, line);
  for (size_t i = start; i < end; i++) {
    char c = buffer_char_at(b, i);
    if (c != ' ' && c != '\t')
      return c;
  }
  return '\0';
}

static int compute_new_indent(Buffer *b, int cur_line, int tab_width) {
  int indent = line_indent_level(b, cur_line);
  char last = line_last_nonspace(b, cur_line);

  if (last == '{' || last == '(' || last == '[' || last == ':')
    indent += tab_width;

  return indent;
}

/* -----------------------------------------------------------------------
 * editor_type_char — main character input handler
 * --------------------------------------------------------------------- */

EditorAction editor_type_char(Editor *e, char c) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;

  if (buffer_has_selection(b))
    buffer_delete_selection(b);

  size_t pos = buffer_cursor(b);

  if (e->auto_pair && is_open(c)) {
    char close = pair_close(c);
    char next = buffer_char_at(b, pos);

    if (c == '"' || c == '\'' || c == '`') {
      if (next == c) {
        buffer_cursor_move(b, 1);
        return ACTION_MOVE;
      }
    }

    char pair[2] = {c, close};
    buffer_insert(b, pos, pair, 2);
    buffer_cursor_set(b, pos + 1);
    return ACTION_INSERT;
  }

  if (e->auto_pair && is_close(c)) {
    char next = buffer_char_at(b, pos);
    if (next == c) {
      buffer_cursor_move(b, 1);
      return ACTION_MOVE;
    }
  }

  buffer_insert_char(b, pos, c);
  buffer_cursor_move(b, 1);
  return ACTION_INSERT;
}

/* -----------------------------------------------------------------------
 * editor_key_return — newline with auto-indent
 * --------------------------------------------------------------------- */

EditorAction editor_key_return(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;

  if (buffer_has_selection(b))
    buffer_delete_selection(b);

  size_t pos = buffer_cursor(b);
  int line, col;
  buffer_pos_to_lc(b, pos, &line, &col);

  buffer_insert_char(b, pos, '\n');
  pos++;

  if (e->auto_indent) {
    int indent = compute_new_indent(b, line, e->tab_width);

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

    char prev_last = line_last_nonspace(b, line);
    if (prev_last == '{') {
      int next_line = line + 1 + (indent > 0 ? 0 : 0);
      char next_first = line_first_nonspace(b, next_line + 1);
      (void)next_first;

      char next_char = buffer_char_at(b, pos);
      if (next_char == '}') {
        buffer_insert_char(b, pos, '\n');
        int base_indent = line_indent_level(b, line);
        char *closing_indent = xmalloc(base_indent + 1);
        if (closing_indent) {
          memset(closing_indent, ' ', base_indent);
          closing_indent[base_indent] = '\0';
          buffer_insert(b, pos + 1, closing_indent, base_indent);
          free(closing_indent);
        }
      }
    }
  }

  buffer_cursor_set(b, pos);
  return ACTION_INSERT;
}

/* -----------------------------------------------------------------------
 * editor_key_backspace
 * --------------------------------------------------------------------- */

EditorAction editor_key_backspace(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;

  if (buffer_has_selection(b)) {
    buffer_delete_selection(b);
    return ACTION_DELETE;
  }

  size_t pos = buffer_cursor(b);
  if (pos == 0)
    return ACTION_NONE;

  char prev = buffer_char_at(b, pos - 1);
  char next = buffer_char_at(b, pos);

  if (e->auto_pair && is_open(prev) && next == pair_close(prev)) {
    buffer_delete(b, pos - 1, 2);
    buffer_cursor_set(b, pos - 1);
    return ACTION_DELETE;
  }

  int line, col;
  buffer_pos_to_lc(b, pos, &line, &col);

  if (e->use_spaces && col > 0) {
    size_t line_start = buffer_line_start(b, line);
    int all_spaces = 1;
    for (size_t i = line_start; i < pos; i++) {
      if (buffer_char_at(b, i) != ' ') {
        all_spaces = 0;
        break;
      }
    }

    if (all_spaces && col % e->tab_width == 0) {
      int del = e->tab_width;
      if (del > col)
        del = col;
      buffer_delete(b, pos - del, del);
      buffer_cursor_set(b, pos - del);
      return ACTION_DELETE;
    }
  }

  buffer_delete_char_before(b, pos);
  buffer_cursor_set(b, pos - 1);
  return ACTION_DELETE;
}

/* -----------------------------------------------------------------------
 * editor_key_tab
 * --------------------------------------------------------------------- */

EditorAction editor_key_tab(Editor *e, int shift) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;

  if (buffer_has_selection(b)) {
    size_t sel_start, sel_end;
    buffer_selection_range(b, &sel_start, &sel_end);

    int start_line, end_line, dummy;
    buffer_pos_to_lc(b, sel_start, &start_line, &dummy);
    buffer_pos_to_lc(b, sel_end, &end_line, &dummy);
    (void)dummy;

    for (int ln = start_line; ln <= end_line; ln++) {
      if (!shift) {
        size_t ls = buffer_line_start(b, ln);
        if (e->use_spaces) {
          char spaces[INDENT_SIZE + 1];
          memset(spaces, ' ', e->tab_width);
          spaces[e->tab_width] = '\0';
          buffer_insert(b, ls, spaces, e->tab_width);
        } else {
          buffer_insert_char(b, ls, '\t');
        }
      } else {
        size_t ls = buffer_line_start(b, ln);
        int removed = 0;
        while (removed < e->tab_width) {
          char c = buffer_char_at(b, ls);
          if (c == ' ') {
            buffer_delete(b, ls, 1);
            removed++;
          } else if (c == '\t') {
            buffer_delete(b, ls, 1);
            removed = e->tab_width;
          } else
            break;
        }
      }
    }
    return ACTION_INDENT;
  }

  size_t pos = buffer_cursor(b);
  if (e->use_spaces) {
    int line, col;
    buffer_pos_to_lc(b, pos, &line, &col);
    int spaces = e->tab_width - (col % e->tab_width);
    char *sp = xmalloc(spaces + 1);
    if (sp) {
      memset(sp, ' ', spaces);
      sp[spaces] = '\0';
      buffer_insert(b, pos, sp, spaces);
      buffer_cursor_move(b, spaces);
      free(sp);
    }
  } else {
    buffer_insert_char(b, pos, '\t');
    buffer_cursor_move(b, 1);
  }
  return ACTION_INSERT;
}

/* -----------------------------------------------------------------------
 * editor_key_delete
 * --------------------------------------------------------------------- */

EditorAction editor_key_delete(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;

  if (buffer_has_selection(b)) {
    buffer_delete_selection(b);
    return ACTION_DELETE;
  }

  buffer_delete_char_after(b, buffer_cursor(b));
  return ACTION_DELETE;
}

/* -----------------------------------------------------------------------
 * editor_move_* — cursor movement with optional selection extension
 * --------------------------------------------------------------------- */

static void maybe_set_mark(Editor *e, int extend) {
  Buffer *b = e->buf;
  if (extend && !buffer_has_selection(b))
    buffer_mark_set(b, buffer_cursor(b));
  if (!extend)
    buffer_mark_clear(b);
}

EditorAction editor_move_left(Editor *e, int extend, int by_word) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  if (by_word)
    buffer_cursor_word_back(e->buf);
  else
    buffer_cursor_move(e->buf, -1);
  return ACTION_MOVE;
}

EditorAction editor_move_right(Editor *e, int extend, int by_word) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  if (by_word)
    buffer_cursor_word_forward(e->buf);
  else
    buffer_cursor_move(e->buf, 1);
  return ACTION_MOVE;
}

EditorAction editor_move_up(Editor *e, int extend, int lines) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  buffer_cursor_line(e->buf, -lines);
  return ACTION_MOVE;
}

EditorAction editor_move_down(Editor *e, int extend, int lines) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  buffer_cursor_line(e->buf, lines);
  return ACTION_MOVE;
}

EditorAction editor_move_home(Editor *e, int extend) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  Buffer *b = e->buf;
  size_t pos = buffer_cursor(b);
  int line, col;
  buffer_pos_to_lc(b, pos, &line, &col);
  size_t line_start = buffer_line_start(b, line);

  size_t first_nonspace = line_start;
  for (size_t i = line_start;; i++) {
    char c = buffer_char_at(b, i);
    if (c == ' ' || c == '\t')
      first_nonspace = i + 1;
    else
      break;
  }

  if (pos == first_nonspace || col == 0)
    buffer_cursor_home(b);
  else
    buffer_cursor_set(b, first_nonspace);

  return ACTION_MOVE;
}

EditorAction editor_move_end(Editor *e, int extend) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  buffer_cursor_end(e->buf);
  return ACTION_MOVE;
}

EditorAction editor_move_doc_start(Editor *e, int extend) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  buffer_cursor_set(e->buf, 0);
  return ACTION_MOVE;
}

EditorAction editor_move_doc_end(Editor *e, int extend) {
  if (!e)
    return ACTION_NONE;
  maybe_set_mark(e, extend);
  buffer_cursor_set(e->buf, buffer_length(e->buf));
  return ACTION_MOVE;
}

EditorAction editor_move_to_line(Editor *e, int line) {
  if (!e)
    return ACTION_NONE;
  buffer_mark_clear(e->buf);
  buffer_cursor_set(e->buf, buffer_lc_to_pos(e->buf, line, 0));
  return ACTION_MOVE;
}

/* -----------------------------------------------------------------------
 * editor_select_all
 * --------------------------------------------------------------------- */

EditorAction editor_select_all(Editor *e) {
  if (!e)
    return ACTION_NONE;
  buffer_mark_set(e->buf, 0);
  buffer_cursor_set(e->buf, buffer_length(e->buf));
  return ACTION_MOVE;
}

EditorAction editor_select_word(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;
  size_t pos = buffer_cursor(b);
  size_t len = buffer_length(b);

  size_t start = pos;
  while (start > 0 && (isalnum((unsigned char)buffer_char_at(b, start - 1)) ||
                       buffer_char_at(b, start - 1) == '_'))
    start--;

  size_t end = pos;
  while (end < len && (isalnum((unsigned char)buffer_char_at(b, end)) ||
                       buffer_char_at(b, end) == '_'))
    end++;

  if (end > start) {
    buffer_mark_set(b, start);
    buffer_cursor_set(b, end);
  }
  return ACTION_MOVE;
}

EditorAction editor_select_line(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;
  int line, col;
  buffer_pos_to_lc(b, buffer_cursor(b), &line, &col);
  (void)col;
  size_t start = buffer_line_start(b, line);
  size_t end = (line + 1 < buffer_line_count(b))
                   ? buffer_line_start(b, line + 1)
                   : buffer_length(b);
  buffer_mark_set(b, start);
  buffer_cursor_set(b, end);
  return ACTION_MOVE;
}

/* -----------------------------------------------------------------------
 * Line operations
 * --------------------------------------------------------------------- */

EditorAction editor_duplicate_line(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;
  int line, col;
  buffer_pos_to_lc(b, buffer_cursor(b), &line, &col);
  (void)col;

  char *text = buffer_line_text(b, line);
  if (!text)
    return ACTION_NONE;

  size_t end = (line + 1 < buffer_line_count(b))
                   ? buffer_line_start(b, line + 1)
                   : buffer_length(b);

  size_t tlen = strlen(text);
  char *with_nl = xmalloc(tlen + 2);
  if (!with_nl) {
    free(text);
    return ACTION_NONE;
  }
  memcpy(with_nl, text, tlen);
  with_nl[tlen] = '\n';
  with_nl[tlen + 1] = '\0';

  buffer_insert(b, end, with_nl, tlen + 1);

  free(text);
  free(with_nl);
  buffer_cursor_set(b, buffer_lc_to_pos(b, line + 1, col));
  return ACTION_INSERT;
}

EditorAction editor_delete_line(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;
  int line, col;
  buffer_pos_to_lc(b, buffer_cursor(b), &line, &col);
  (void)col;
  buffer_delete_line(b, line);
  int new_line = line < buffer_line_count(b) ? line : line - 1;
  if (new_line < 0)
    new_line = 0;
  buffer_cursor_set(b, buffer_lc_to_pos(b, new_line, 0));
  return ACTION_DELETE;
}

EditorAction editor_move_line_up(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;
  int line, col;
  buffer_pos_to_lc(b, buffer_cursor(b), &line, &col);
  if (line == 0)
    return ACTION_NONE;

  char *cur = buffer_line_text(b, line);
  char *prev = buffer_line_text(b, line - 1);
  if (!cur || !prev) {
    free(cur);
    free(prev);
    return ACTION_NONE;
  }

  size_t cur_len = strlen(cur);
  size_t prev_len = strlen(prev);

  size_t prev_start = buffer_line_start(b, line - 1);
  size_t block_len = prev_len + 1 + cur_len + 1;

  char *swapped = xmalloc(block_len + 1);
  if (!swapped) {
    free(cur);
    free(prev);
    return ACTION_NONE;
  }

  int n = snprintf(swapped, block_len + 1, "%s\n%s\n", cur, prev);
  buffer_delete(b, prev_start, (size_t)n > block_len ? block_len : (size_t)n);
  buffer_insert(b, prev_start, swapped,
                (size_t)n > block_len ? block_len : (size_t)n);
  buffer_cursor_set(b, buffer_lc_to_pos(b, line - 1, col));

  free(cur);
  free(prev);
  free(swapped);
  return ACTION_INSERT;
}

EditorAction editor_move_line_down(Editor *e) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;
  int line, col;
  buffer_pos_to_lc(b, buffer_cursor(b), &line, &col);
  if (line + 1 >= buffer_line_count(b))
    return ACTION_NONE;

  char *cur = buffer_line_text(b, line);
  char *next = buffer_line_text(b, line + 1);
  if (!cur || !next) {
    free(cur);
    free(next);
    return ACTION_NONE;
  }

  size_t cur_len = strlen(cur);
  size_t next_len = strlen(next);
  size_t cur_start = buffer_line_start(b, line);
  size_t block_len = cur_len + 1 + next_len + 1;

  char *swapped = xmalloc(block_len + 1);
  if (!swapped) {
    free(cur);
    free(next);
    return ACTION_NONE;
  }

  int n = snprintf(swapped, block_len + 1, "%s\n%s\n", next, cur);
  buffer_delete(b, cur_start, (size_t)n > block_len ? block_len : (size_t)n);
  buffer_insert(b, cur_start, swapped,
                (size_t)n > block_len ? block_len : (size_t)n);
  buffer_cursor_set(b, buffer_lc_to_pos(b, line + 1, col));

  free(cur);
  free(next);
  free(swapped);
  return ACTION_INSERT;
}

/* -----------------------------------------------------------------------
 * Clipboard
 * --------------------------------------------------------------------- */

EditorAction editor_copy(Editor *e) {
  if (!e)
    return ACTION_NONE;
  char *text;
  size_t len;
  if (buffer_copy(e->buf, &text, &len) < 0)
    return ACTION_NONE;
  free(e->clipboard);
  e->clipboard = text;
  e->clipboard_len = len;
  return ACTION_COPY;
}

EditorAction editor_cut(Editor *e) {
  if (!e)
    return ACTION_NONE;
  char *text;
  size_t len;
  if (buffer_cut(e->buf, &text, &len) < 0)
    return ACTION_NONE;
  free(e->clipboard);
  e->clipboard = text;
  e->clipboard_len = len;
  return ACTION_CUT;
}

EditorAction editor_paste(Editor *e) {
  if (!e || !e->clipboard)
    return ACTION_NONE;
  buffer_paste(e->buf, e->clipboard, e->clipboard_len);
  return ACTION_INSERT;
}

EditorAction editor_paste_text(Editor *e, const char *text, size_t len) {
  if (!e || !text || len == 0)
    return ACTION_NONE;
  buffer_paste(e->buf, text, len);
  return ACTION_INSERT;
}

/* -----------------------------------------------------------------------
 * Undo / redo
 * --------------------------------------------------------------------- */

EditorAction editor_undo(Editor *e) {
  if (!e)
    return ACTION_NONE;
  if (buffer_undo(e->buf) < 0)
    return ACTION_NONE;
  return ACTION_UNDO;
}

EditorAction editor_redo(Editor *e) {
  if (!e)
    return ACTION_NONE;
  if (buffer_redo(e->buf) < 0)
    return ACTION_NONE;
  return ACTION_REDO;
}

/* -----------------------------------------------------------------------
 * Comment toggling
 * --------------------------------------------------------------------- */

static const char *comment_prefix(SyntaxLang lang) {
  switch (lang) {
  case LANG_C:
  case LANG_JS:
    return "// ";
  case LANG_PYTHON:
  case LANG_SHELL:
  case LANG_MAKEFILE:
    return "# ";
  case LANG_ASM:
    return "; ";
  default:
    return "// ";
  }
}

EditorAction editor_toggle_comment(Editor *e, SyntaxLang lang) {
  if (!e)
    return ACTION_NONE;
  Buffer *b = e->buf;

  int start_line, end_line, dummy;
  if (buffer_has_selection(b)) {
    size_t sel_start, sel_end;
    buffer_selection_range(b, &sel_start, &sel_end);
    buffer_pos_to_lc(b, sel_start, &start_line, &dummy);
    buffer_pos_to_lc(b, sel_end, &end_line, &dummy);
    (void)dummy;
  } else {
    buffer_pos_to_lc(b, buffer_cursor(b), &start_line, &dummy);
    end_line = start_line;
  }

  const char *prefix = comment_prefix(lang);
  size_t prefix_len = strlen(prefix);

  int all_commented = 1;
  for (int ln = start_line; ln <= end_line; ln++) {
    char *text = buffer_line_text(b, ln);
    if (!text)
      continue;
    int has = strncmp(text, prefix, prefix_len) == 0;
    free(text);
    if (!has) {
      all_commented = 0;
      break;
    }
  }

  for (int ln = start_line; ln <= end_line; ln++) {
    size_t ls = buffer_line_start(b, ln);
    if (all_commented) {
      char *text = buffer_line_text(b, ln);
      if (text && strncmp(text, prefix, prefix_len) == 0) {
        buffer_delete(b, ls, prefix_len);
      }
      free(text);
    } else {
      buffer_insert(b, ls, prefix, prefix_len);
    }
  }

  return ACTION_INSERT;
}

/* -----------------------------------------------------------------------
 * Bracket matching
 * --------------------------------------------------------------------- */

size_t editor_find_matching_bracket(Editor *e) {
  if (!e)
    return BUFFER_NPOS;
  Buffer *b = e->buf;
  size_t pos = buffer_cursor(b);
  size_t len = buffer_length(b);

  char c = buffer_char_at(b, pos);

  char open = 0, close = 0;
  int forward = 1;

  if (c == '(' || c == '[' || c == '{') {
    open = c;
    close = (c == '(') ? ')' : (c == '[') ? ']' : '}';
    forward = 1;
  } else if (c == ')' || c == ']' || c == '}') {
    close = c;
    open = (c == ')') ? '(' : (c == ']') ? '[' : '{';
    forward = 0;
  } else {
    return BUFFER_NPOS;
  }

  int depth = 1;
  if (forward) {
    for (size_t i = pos + 1; i < len; i++) {
      char ch = buffer_char_at(b, i);
      if (ch == open)
        depth++;
      if (ch == close) {
        depth--;
        if (depth == 0)
          return i;
      }
    }
  } else {
    for (size_t i = pos; i > 0; i--) {
      char ch = buffer_char_at(b, i - 1);
      if (ch == close)
        depth++;
      if (ch == open) {
        depth--;
        if (depth == 0)
          return i - 1;
      }
    }
  }
  return BUFFER_NPOS;
}

/* -----------------------------------------------------------------------
 * Word count
 * --------------------------------------------------------------------- */

EditorStats editor_stats(Editor *e) {
  EditorStats s = {0};
  if (!e)
    return s;

  Buffer *b = e->buf;
  size_t len = buffer_length(b);
  s.char_count = (int)len;
  s.line_count = buffer_line_count(b);

  int in_word = 0;
  for (size_t i = 0; i < len; i++) {
    char c = buffer_char_at(b, i);
    if (isspace((unsigned char)c)) {
      in_word = 0;
    } else {
      if (!in_word) {
        s.word_count++;
        in_word = 1;
      }
    }
  }

  int line, col;
  buffer_pos_to_lc(b, buffer_cursor(b), &line, &col);
  s.cursor_line = line + 1;
  s.cursor_col = col + 1;

  return s;
}