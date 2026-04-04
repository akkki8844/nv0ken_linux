#include "../../../libnv0/include/nv0/app.h"
#include "../../../libnv0/include/nv0/dialog.h"
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../libnv0/include/nv0/input.h"
#include "../../../libnv0/include/nv0/window.h"
#include "buffer.h"
#include "editor.h"
#include "syntax.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define WINDOW_W 1000
#define WINDOW_H 680
#define TOOLBAR_H 36
#define STATUSBAR_H 22
#define GUTTER_W 48
#define CELL_W 8
#define CELL_H 16
#define PADDING 8
#define CONTENT_X (GUTTER_W + PADDING)
#define CONTENT_Y TOOLBAR_H
#define CONTENT_W (WINDOW_W - GUTTER_W - PADDING)
#define CONTENT_H (WINDOW_H - TOOLBAR_H - STATUSBAR_H)
#define VISIBLE_COLS ((CONTENT_W) / CELL_W)
#define VISIBLE_ROWS ((CONTENT_H) / CELL_H)

#define COL_BG NV_COLOR(0x1E, 0x1E, 0x1E, 0xFF)
#define COL_GUTTER_BG NV_COLOR(0x25, 0x25, 0x25, 0xFF)
#define COL_GUTTER_FG NV_COLOR(0x50, 0x50, 0x50, 0xFF)
#define COL_CURLINE_BG NV_COLOR(0x28, 0x28, 0x28, 0xFF)
#define COL_CURSOR NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_TOOLBAR_BG NV_COLOR(0x2D, 0x2D, 0x2D, 0xFF)
#define COL_STATUS_BG NV_COLOR(0x00, 0x7A, 0xCC, 0xFF)
#define COL_STATUS_FG NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF)
#define COL_BTN_BG NV_COLOR(0x40, 0x40, 0x40, 0xFF)
#define COL_BTN_FG NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_BORDER NV_COLOR(0x14, 0x14, 0x14, 0xFF)
#define COL_SELECTION NV_COLOR(0x26, 0x4F, 0x78, 0xFF)
#define COL_FIND_HL NV_COLOR(0x61, 0x3F, 0x00, 0xFF)

static const NvColor SYN_COLORS[] = {
    NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF), NV_COLOR(0x56, 0x9C, 0xD6, 0xFF),
    NV_COLOR(0x4E, 0xC9, 0xB0, 0xFF), NV_COLOR(0xDC, 0xDC, 0xAA, 0xFF),
    NV_COLOR(0xCE, 0x91, 0x78, 0xFF), NV_COLOR(0xB5, 0xCE, 0xA8, 0xFF),
    NV_COLOR(0x6A, 0x99, 0x55, 0xFF), NV_COLOR(0xC5, 0x86, 0xC0, 0xFF),
    NV_COLOR(0x9C, 0xDC, 0xFE, 0xFF), NV_COLOR(0xD7, 0xBA, 0x7D, 0xFF),
    NV_COLOR(0xFF, 0xD7, 0x00, 0xFF), NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF),
    NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF), NV_COLOR(0x56, 0x9C, 0xD6, 0xFF),
    NV_COLOR(0x4E, 0xC9, 0xB0, 0xFF), NV_COLOR(0xDC, 0xDC, 0xAA, 0xFF),
};

typedef struct {
  NvWindow *window;
  NvSurface *surface;

  Buffer *buf;
  Editor *editor;
  SyntaxState *syntax;
  SyntaxLang lang;

  int scroll_line;
  int scroll_col;

  char find_query[256];
  int find_active;
  size_t find_pos;

  char status_msg[256];
} TextEditor;

static TextEditor g_ed;

/* -----------------------------------------------------------------------
 * Scroll management
 * --------------------------------------------------------------------- */

static void ensure_cursor_visible(TextEditor *te) {
  int cur_line, cur_col;
  buffer_pos_to_lc(te->buf, buffer_cursor(te->buf), &cur_line, &cur_col);

  if (cur_line < te->scroll_line)
    te->scroll_line = cur_line;
  if (cur_line >= te->scroll_line + VISIBLE_ROWS)
    te->scroll_line = cur_line - VISIBLE_ROWS + 1;

  if (cur_col < te->scroll_col)
    te->scroll_col = cur_col;
  if (cur_col >= te->scroll_col + VISIBLE_COLS)
    te->scroll_col = cur_col - VISIBLE_COLS + 1;
}

/* -----------------------------------------------------------------------
 * Status
 * --------------------------------------------------------------------- */

static void set_status(TextEditor *te, const char *msg) {
  strncpy(te->status_msg, msg, sizeof(te->status_msg) - 1);
  te->status_msg[sizeof(te->status_msg) - 1] = '\0';
  nv_surface_invalidate(te->surface);
}

static void update_status(TextEditor *te) {
  int line, col;
  buffer_pos_to_lc(te->buf, buffer_cursor(te->buf), &line, &col);

  char buf[256];
  const char *filename = buffer_filename(te->buf);
  if (!filename)
    filename = "untitled";
  snprintf(buf, sizeof(buf), "%s%s  Ln %d, Col %d  %s", filename,
           buffer_modified(te->buf) ? " *" : "", line + 1, col + 1,
           te->lang != LANG_NONE ? "" : "");
  strncpy(te->status_msg, buf, sizeof(te->status_msg) - 1);
}

/* -----------------------------------------------------------------------
 * File operations
 * --------------------------------------------------------------------- */

static void file_new(TextEditor *te) {
  if (buffer_modified(te->buf)) {
    if (nv_dialog_confirm("Unsaved changes",
                          "Discard changes and open a new file?") != 1)
      return;
  }
  buffer_free(te->buf);
  te->buf = buffer_new();
  syntax_state_free(te->syntax);
  te->lang = LANG_NONE;
  te->syntax = syntax_state_new(LANG_NONE);
  te->scroll_line = 0;
  te->scroll_col = 0;
  nv_window_set_title(te->window, "Text Editor — untitled");
  update_status(te);
  nv_surface_invalidate(te->surface);
}

static void file_open(TextEditor *te, const char *path) {
  if (buffer_modified(te->buf)) {
    if (nv_dialog_confirm("Unsaved changes",
                          "Discard changes and open file?") != 1)
      return;
  }
  if (buffer_load(te->buf, path) < 0) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Failed to open: %s", path);
    set_status(te, msg);
    return;
  }
  syntax_state_free(te->syntax);
  te->lang = syntax_detect(path);
  te->syntax = syntax_state_new(te->lang);
  te->scroll_line = 0;
  te->scroll_col = 0;

  char title[512];
  const char *name = strrchr(path, '/');
  name = name ? name + 1 : path;
  snprintf(title, sizeof(title), "Text Editor — %s", name);
  nv_window_set_title(te->window, title);

  update_status(te);
  nv_surface_invalidate(te->surface);
}

static void file_open_dialog(TextEditor *te) {
  char path[1024] = {0};
  if (nv_dialog_open_file("Open File", path, sizeof(path), "*") != 1)
    return;
  if (path[0])
    file_open(te, path);
}

static void file_save(TextEditor *te) {
  if (!buffer_filename(te->buf)) {
    char path[1024] = {0};
    if (nv_dialog_save_file("Save File", path, sizeof(path)) != 1)
      return;
    if (!path[0])
      return;
    if (buffer_save(te->buf, path) < 0) {
      set_status(te, "Save failed");
      return;
    }
  } else {
    if (buffer_save(te->buf, NULL) < 0) {
      set_status(te, "Save failed");
      return;
    }
  }
  update_status(te);
  nv_surface_invalidate(te->surface);
}

static void file_save_as(TextEditor *te) {
  char path[1024] = {0};
  if (nv_dialog_save_file("Save As", path, sizeof(path)) != 1)
    return;
  if (!path[0])
    return;
  if (buffer_save(te->buf, path) < 0) {
    set_status(te, "Save failed");
    return;
  }
  syntax_state_free(te->syntax);
  te->lang = syntax_detect(path);
  te->syntax = syntax_state_new(te->lang);
  update_status(te);
  nv_surface_invalidate(te->surface);
}

/* -----------------------------------------------------------------------
 * Find
 * --------------------------------------------------------------------- */

static void find_next(TextEditor *te) {
  if (!te->find_query[0])
    return;
  size_t len = strlen(te->find_query);
  size_t from = buffer_cursor(te->buf) + 1;
  size_t found = buffer_find(te->buf, from, te->find_query, len, 0);
  if (found == BUFFER_NPOS)
    found = buffer_find(te->buf, 0, te->find_query, len, 0);
  if (found != BUFFER_NPOS) {
    buffer_cursor_set(te->buf, found);
    te->find_pos = found;
    ensure_cursor_visible(te);
    nv_surface_invalidate(te->surface);
  } else {
    set_status(te, "Not found");
  }
}

static void find_prev(TextEditor *te) {
  if (!te->find_query[0])
    return;
  size_t len = strlen(te->find_query);
  size_t from = buffer_cursor(te->buf);
  size_t found = buffer_find_prev(te->buf, from, te->find_query, len, 0);
  if (found != BUFFER_NPOS) {
    buffer_cursor_set(te->buf, found);
    te->find_pos = found;
    ensure_cursor_visible(te);
    nv_surface_invalidate(te->surface);
  } else {
    set_status(te, "Not found");
  }
}

static void find_prompt(TextEditor *te) {
  char query[256] = {0};
  if (nv_dialog_input("Find", "Search for:", query, sizeof(query)) != 1)
    return;
  if (!query[0])
    return;
  strncpy(te->find_query, query, sizeof(te->find_query) - 1);
  te->find_active = 1;
  te->find_pos = BUFFER_NPOS;
  find_next(te);
}

static void replace_prompt(TextEditor *te) {
  char needle[256] = {0};
  char replacement[256] = {0};
  if (nv_dialog_input("Find", "Find:", needle, sizeof(needle)) != 1)
    return;
  if (!needle[0])
    return;
  if (nv_dialog_input("Replace", "Replace with:", replacement,
                      sizeof(replacement)) != 1)
    return;

  int count = buffer_replace_all(te->buf, needle, strlen(needle), replacement,
                                 strlen(replacement), 0);
  char msg[64];
  snprintf(msg, sizeof(msg), "Replaced %d occurrence%s", count,
           count == 1 ? "" : "s");
  set_status(te, msg);
  ensure_cursor_visible(te);
  nv_surface_invalidate(te->surface);
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_toolbar(TextEditor *te) {
  NvRect bg = {0, 0, WINDOW_W, TOOLBAR_H};
  nv_draw_fill_rect(te->surface, &bg, COL_TOOLBAR_BG);
  NvRect border = {0, TOOLBAR_H - 1, WINDOW_W, 1};
  nv_draw_fill_rect(te->surface, &border, COL_BORDER);

  struct {
    int x;
    const char *label;
  } btns[] = {
      {4, "New"},       {52, "Open"},     {104, "Save"},
      {152, "Save As"}, {232, "Undo"},    {280, "Redo"},
      {328, "Find"},    {372, "Replace"}, {-1, NULL},
  };

  for (int i = 0; btns[i].label; i++) {
    int bw = (int)strlen(btns[i].label) * 8 + 14;
    NvRect r = {btns[i].x, 4, bw, 28};
    nv_draw_fill_rect(te->surface, &r, COL_BTN_BG);
    nv_draw_rect(te->surface, &r, COL_BORDER);
    nv_draw_text(te->surface, btns[i].x + 7, 11, btns[i].label, COL_BTN_FG);
  }
}

static void draw_gutter(TextEditor *te, int first_line, int last_line) {
  NvRect gutter = {0, CONTENT_Y, GUTTER_W, CONTENT_H};
  nv_draw_fill_rect(te->surface, &gutter, COL_GUTTER_BG);
  NvRect border = {GUTTER_W - 1, CONTENT_Y, 1, CONTENT_H};
  nv_draw_fill_rect(te->surface, &border, COL_BORDER);

  int cur_line, cur_col;
  buffer_pos_to_lc(te->buf, buffer_cursor(te->buf), &cur_line, &cur_col);

  for (int i = first_line; i < last_line; i++) {
    int y = CONTENT_Y + (i - first_line) * CELL_H;
    char num[16];
    snprintf(num, sizeof(num), "%d", i + 1);
    int nw = (int)strlen(num) * CELL_W;
    NvColor fg =
        (i == cur_line) ? NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF) : COL_GUTTER_FG;
    nv_draw_text(te->surface, GUTTER_W - nw - 6, y + 2, num, fg);
  }
}

static void draw_content(TextEditor *te) {
  int line_count = buffer_line_count(te->buf);
  int first_line = te->scroll_line;
  int last_line = first_line + VISIBLE_ROWS;
  if (last_line > line_count)
    last_line = line_count;

  int cur_line, cur_col;
  buffer_pos_to_lc(te->buf, buffer_cursor(te->buf), &cur_line, &cur_col);

  size_t sel_start = 0, sel_end = 0;
  int has_sel = buffer_has_selection(te->buf);
  if (has_sel)
    buffer_selection_range(te->buf, &sel_start, &sel_end);

  syntax_reset(te->syntax);

  for (int row = 0; row < first_line; row++) {
    char *text = buffer_line_text(te->buf, row);
    if (text) {
      syntax_highlight_line(te->syntax, text, (int)strlen(text));
      free(text);
    }
  }

  for (int row = first_line; row < last_line; row++) {
    int screen_y = CONTENT_Y + (row - first_line) * CELL_H;

    if (row == cur_line) {
      NvRect hl = {GUTTER_W, screen_y, WINDOW_W - GUTTER_W, CELL_H};
      nv_draw_fill_rect(te->surface, &hl, COL_CURLINE_BG);
    }

    char *text = buffer_line_text(te->buf, row);
    int text_len = text ? (int)strlen(text) : 0;

    if (text)
      syntax_highlight_line(te->syntax, text, text_len);

    size_t line_start = buffer_line_start(te->buf, row);
    size_t line_end = buffer_line_end(te->buf, row);

    for (int col = te->scroll_col; col < te->scroll_col + VISIBLE_COLS; col++) {
      int screen_x = CONTENT_X + (col - te->scroll_col) * CELL_W;
      size_t char_pos = line_start + col;

      int in_sel = has_sel && char_pos >= sel_start && char_pos < sel_end;
      int in_find = te->find_active && te->find_query[0] &&
                    te->find_pos != BUFFER_NPOS && char_pos >= te->find_pos &&
                    char_pos < te->find_pos + strlen(te->find_query);

      NvColor bg = COL_BG;
      if (row == cur_line)
        bg = COL_CURLINE_BG;
      if (in_sel)
        bg = COL_SELECTION;
      if (in_find)
        bg = COL_FIND_HL;

      NvRect cell_rect = {screen_x, screen_y, CELL_W, CELL_H};
      nv_draw_fill_rect(te->surface, &cell_rect, bg);

      if (text && col < text_len) {
        SyntaxTokenType tok = syntax_token_at(te->syntax, col);
        NvColor fg = SYN_COLORS[tok < 16 ? tok : 0];
        char glyph[2] = {text[col], '\0'};
        int bold = (tok == TOK_KEYWORD || tok == TOK_HEADING1);
        nv_draw_text_styled(te->surface, screen_x, screen_y, glyph, fg, CELL_H,
                            bold, 0);
      }

      if (row == cur_line && col == cur_col) {
        NvRect cur_rect = {screen_x, screen_y + CELL_H - 2, CELL_W, 2};
        nv_draw_fill_rect(te->surface, &cur_rect, COL_CURSOR);
      }
    }

    free(text);
  }

  draw_gutter(te, first_line, last_line);
}

static void draw_statusbar(TextEditor *te) {
  int y = WINDOW_H - STATUSBAR_H;
  NvRect bg = {0, y, WINDOW_W, STATUSBAR_H};
  nv_draw_fill_rect(te->surface, &bg, COL_STATUS_BG);
  nv_draw_text(te->surface, 8, y + 3, te->status_msg, COL_STATUS_FG);
}

static void on_paint(NvWindow *win, NvSurface *surface, void *userdata) {
  (void)win;
  TextEditor *te = (TextEditor *)userdata;
  te->surface = surface;

  NvRect bg = {0, 0, WINDOW_W, WINDOW_H};
  nv_draw_fill_rect(surface, &bg, COL_BG);

  draw_toolbar(te);
  draw_content(te);
  draw_statusbar(te);
}

/* -----------------------------------------------------------------------
 * Toolbar hit test
 * --------------------------------------------------------------------- */

typedef enum {
  TB_NONE,
  TB_NEW,
  TB_OPEN,
  TB_SAVE,
  TB_SAVE_AS,
  TB_UNDO,
  TB_REDO,
  TB_FIND,
  TB_REPLACE,
} ToolbarBtn;

static ToolbarBtn toolbar_hit(int x, int y) {
  if (y < 4 || y > 32)
    return TB_NONE;
  if (x >= 4 && x < 48)
    return TB_NEW;
  if (x >= 52 && x < 100)
    return TB_OPEN;
  if (x >= 104 && x < 148)
    return TB_SAVE;
  if (x >= 152 && x < 228)
    return TB_SAVE_AS;
  if (x >= 232 && x < 276)
    return TB_UNDO;
  if (x >= 280 && x < 324)
    return TB_REDO;
  if (x >= 328 && x < 368)
    return TB_FIND;
  if (x >= 372 && x < 444)
    return TB_REPLACE;
  return TB_NONE;
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

static void on_mouse_down(NvWindow *win, NvMouseEvent *ev, void *userdata) {
  (void)win;
  TextEditor *te = (TextEditor *)userdata;

  if (ev->y < TOOLBAR_H) {
    ToolbarBtn btn = toolbar_hit(ev->x, ev->y);
    switch (btn) {
    case TB_NEW:
      file_new(te);
      break;
    case TB_OPEN:
      file_open_dialog(te);
      break;
    case TB_SAVE:
      file_save(te);
      break;
    case TB_SAVE_AS:
      file_save_as(te);
      break;
    case TB_UNDO:
      buffer_undo(te->buf);
      break;
    case TB_REDO:
      buffer_redo(te->buf);
      break;
    case TB_FIND:
      find_prompt(te);
      break;
    case TB_REPLACE:
      replace_prompt(te);
      break;
    default:
      break;
    }
    update_status(te);
    nv_surface_invalidate(te->surface);
    return;
  }

  if (ev->x < GUTTER_W)
    return;

  int col = (ev->x - CONTENT_X) / CELL_W + te->scroll_col;
  int row = (ev->y - CONTENT_Y) / CELL_H + te->scroll_line;
  size_t pos = buffer_lc_to_pos(te->buf, row, col);

  if (ev->mod & NV_MOD_SHIFT) {
    if (!buffer_has_selection(te->buf))
      buffer_mark_set(te->buf, buffer_cursor(te->buf));
    buffer_cursor_set(te->buf, pos);
  } else {
    buffer_mark_clear(te->buf);
    buffer_cursor_set(te->buf, pos);
  }

  update_status(te);
  nv_surface_invalidate(te->surface);
}

static void on_scroll(NvWindow *win, NvScrollEvent *ev, void *userdata) {
  (void)win;
  (void)ev;
  TextEditor *te = (TextEditor *)userdata;
  te->scroll_line += ev->delta_y * 3;
  if (te->scroll_line < 0)
    te->scroll_line = 0;
  int max = buffer_line_count(te->buf) - 1;
  if (te->scroll_line > max)
    te->scroll_line = max;
  nv_surface_invalidate(te->surface);
}

static void on_key_down(NvWindow *win, NvKeyEvent *ev, void *userdata) {
  (void)win;
  TextEditor *te = (TextEditor *)userdata;
  Buffer *b = te->buf;

  if (ev->mod & NV_MOD_CTRL) {
    switch (ev->key) {
    case 'n':
    case 'N':
      file_new(te);
      return;
    case 'o':
    case 'O':
      file_open_dialog(te);
      return;
    case 's':
    case 'S':
      if (ev->mod & NV_MOD_SHIFT)
        file_save_as(te);
      else
        file_save(te);
      return;
    case 'z':
    case 'Z':
      buffer_undo(b);
      break;
    case 'y':
    case 'Y':
      buffer_redo(b);
      break;
    case 'a':
    case 'A':
      buffer_mark_set(b, 0);
      buffer_cursor_set(b, buffer_length(b));
      break;
    case 'c':
    case 'C': {
      char *text;
      size_t len;
      if (buffer_copy(b, &text, &len) == 0) {
        nv_clipboard_set(text);
        free(text);
      }
      break;
    }
    case 'x':
    case 'X': {
      char *text;
      size_t len;
      if (buffer_cut(b, &text, &len) == 0) {
        nv_clipboard_set(text);
        free(text);
      }
      break;
    }
    case 'v':
    case 'V': {
      char *text = nv_clipboard_get();
      if (text) {
        buffer_paste(b, text, strlen(text));
        free(text);
      }
      break;
    }
    case 'f':
    case 'F':
      find_prompt(te);
      return;
    case 'h':
    case 'H':
      replace_prompt(te);
      return;
    case 'g':
    case 'G': {
      char line_str[32] = {0};
      if (nv_dialog_input("Go to line", "Line number:", line_str,
                          sizeof(line_str)) == 1) {
        int ln = atoi(line_str) - 1;
        buffer_cursor_set(b, buffer_lc_to_pos(b, ln, 0));
        ensure_cursor_visible(te);
      }
      break;
    }
    default:
      break;
    }
    ensure_cursor_visible(te);
    update_status(te);
    nv_surface_invalidate(te->surface);
    return;
  }

  switch (ev->key) {
  case NV_KEY_RETURN: {
    if (buffer_has_selection(b))
      buffer_delete_selection(b);
    size_t pos = buffer_cursor(b);
    buffer_insert_char(b, pos, '\n');
    buffer_cursor_set(b, pos + 1);
    break;
  }
  case NV_KEY_BACKSPACE:
    if (buffer_has_selection(b)) {
      buffer_delete_selection(b);
    } else {
      size_t pos = buffer_cursor(b);
      if (pos > 0) {
        buffer_delete_char_before(b, pos);
        buffer_cursor_set(b, pos - 1);
      }
    }
    break;
  case NV_KEY_DELETE:
    if (buffer_has_selection(b)) {
      buffer_delete_selection(b);
    } else {
      buffer_delete_char_after(b, buffer_cursor(b));
    }
    break;
  case NV_KEY_TAB:
    if (buffer_has_selection(b))
      buffer_delete_selection(b);
    buffer_insert(b, buffer_cursor(b), "    ", 4);
    buffer_cursor_move(b, 4);
    break;
  case NV_KEY_LEFT:
    if (ev->mod & NV_MOD_SHIFT) {
      if (!buffer_has_selection(b))
        buffer_mark_set(b, buffer_cursor(b));
      buffer_cursor_move(b, -1);
    } else {
      buffer_mark_clear(b);
      if (ev->mod & NV_MOD_CTRL)
        buffer_cursor_word_back(b);
      else
        buffer_cursor_move(b, -1);
    }
    break;
  case NV_KEY_RIGHT:
    if (ev->mod & NV_MOD_SHIFT) {
      if (!buffer_has_selection(b))
        buffer_mark_set(b, buffer_cursor(b));
      buffer_cursor_move(b, 1);
    } else {
      buffer_mark_clear(b);
      if (ev->mod & NV_MOD_CTRL)
        buffer_cursor_word_forward(b);
      else
        buffer_cursor_move(b, 1);
    }
    break;
  case NV_KEY_UP:
    if (ev->mod & NV_MOD_SHIFT) {
      if (!buffer_has_selection(b))
        buffer_mark_set(b, buffer_cursor(b));
    } else {
      buffer_mark_clear(b);
    }
    buffer_cursor_line(b, -1);
    break;
  case NV_KEY_DOWN:
    if (ev->mod & NV_MOD_SHIFT) {
      if (!buffer_has_selection(b))
        buffer_mark_set(b, buffer_cursor(b));
    } else {
      buffer_mark_clear(b);
    }
    buffer_cursor_line(b, 1);
    break;
  case NV_KEY_HOME:
    if (ev->mod & NV_MOD_SHIFT) {
      if (!buffer_has_selection(b))
        buffer_mark_set(b, buffer_cursor(b));
    } else {
      buffer_mark_clear(b);
    }
    buffer_cursor_home(b);
    break;
  case NV_KEY_END:
    if (ev->mod & NV_MOD_SHIFT) {
      if (!buffer_has_selection(b))
        buffer_mark_set(b, buffer_cursor(b));
    } else {
      buffer_mark_clear(b);
    }
    buffer_cursor_end(b);
    break;
  case NV_KEY_PAGE_UP:
    buffer_cursor_line(b, -VISIBLE_ROWS);
    te->scroll_line -= VISIBLE_ROWS;
    if (te->scroll_line < 0)
      te->scroll_line = 0;
    break;
  case NV_KEY_PAGE_DOWN:
    buffer_cursor_line(b, VISIBLE_ROWS);
    break;
  case NV_KEY_F3:
    if (ev->mod & NV_MOD_SHIFT)
      find_prev(te);
    else
      find_next(te);
    return;
  default:
    if (ev->codepoint >= 32 && ev->codepoint < 127) {
      if (buffer_has_selection(b))
        buffer_delete_selection(b);
      size_t pos = buffer_cursor(b);
      char c = (char)ev->codepoint;
      buffer_insert_char(b, pos, c);
      buffer_cursor_move(b, 1);
    }
    break;
  }

  ensure_cursor_visible(te);
  update_status(te);
  nv_surface_invalidate(te->surface);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(int argc, char **argv) {
  memset(&g_ed, 0, sizeof(TextEditor));

  g_ed.buf = buffer_new();
  g_ed.lang = LANG_NONE;
  g_ed.syntax = syntax_state_new(LANG_NONE);
  g_ed.find_pos = BUFFER_NPOS;

  if (!g_ed.buf || !g_ed.syntax)
    return 1;

  NvWindowConfig cfg;
  cfg.title = "Text Editor";
  cfg.width = WINDOW_W;
  cfg.height = WINDOW_H;
  cfg.resizable = 0;

  g_ed.window = nv_window_create(&cfg);
  if (!g_ed.window)
    return 1;

  nv_window_on_paint(g_ed.window, on_paint, &g_ed);
  nv_window_on_mouse_down(g_ed.window, on_mouse_down, &g_ed);
  nv_window_on_scroll(g_ed.window, on_scroll, &g_ed);
  nv_window_on_key_down(g_ed.window, on_key_down, &g_ed);

  if (argc > 1)
    file_open(&g_ed, argv[1]);
  update_status(&g_ed);

  return nv_app_run();
}