#include "vt100.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_PARAMS 16
#define MAX_INTERM 4
#define MAX_OSC_LEN 512
#define DEFAULT_FG VT_COLOR_DEFAULT
#define DEFAULT_BG VT_COLOR_DEFAULT

static void *xmalloc(size_t n) {
  void *p = malloc(n);
  if (!p)
    return NULL;
  return p;
}

static void *xrealloc(void *p, size_t n) {
  void *q = realloc(p, n);
  if (!q)
    return NULL;
  return q;
}

/* -----------------------------------------------------------------------
 * VtCell helpers
 * --------------------------------------------------------------------- */

static VtCell cell_blank(void) {
  VtCell c;
  c.codepoint = ' ';
  c.fg = DEFAULT_FG;
  c.bg = DEFAULT_BG;
  c.attrs = 0;
  return c;
}

/* -----------------------------------------------------------------------
 * Scrollback
 * --------------------------------------------------------------------- */

typedef struct {
  VtCell *cells;
  int cols;
} ScrollbackLine;

/* -----------------------------------------------------------------------
 * Vt100 struct
 * --------------------------------------------------------------------- */

struct Vt100 {
  int cols;
  int rows;

  VtCell *screen;
  VtCell *alt_screen;
  int use_alt;

  int cursor_col;
  int cursor_row;
  int saved_col;
  int saved_row;
  int alt_saved_col;
  int alt_saved_row;

  int scroll_top;
  int scroll_bot;

  VtCell current_attr;

  int mode_origin;
  int mode_auto_wrap;
  int mode_insert;
  int mode_cursor_visible;
  int mode_bracketed_paste;
  int mode_alt_screen;

  int pending_wrap;

  VtParseState parse_state;
  int params[MAX_PARAMS];
  int param_count;
  int current_param;
  char interm[MAX_INTERM];
  int interm_count;
  char osc_buf[MAX_OSC_LEN];
  int osc_len;

  ScrollbackLine *scrollback;
  int sb_cap;
  int sb_count;
  int sb_head;
};

/* -----------------------------------------------------------------------
 * Screen access
 * --------------------------------------------------------------------- */

static VtCell *screen_cell(Vt100 *v, int col, int row) {
  VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
  if (col < 0 || col >= v->cols || row < 0 || row >= v->rows)
    return NULL;
  return &screen[row * v->cols + col];
}

static void screen_clear_line(Vt100 *v, int row, int col_start, int col_end) {
  VtCell blank = cell_blank();
  blank.fg = v->current_attr.fg;
  blank.bg = v->current_attr.bg;
  for (int c = col_start; c <= col_end && c < v->cols; c++) {
    VtCell *cell = screen_cell(v, c, row);
    if (cell)
      *cell = blank;
  }
}

static void screen_clear_rows(Vt100 *v, int row_start, int row_end) {
  for (int r = row_start; r <= row_end && r < v->rows; r++)
    screen_clear_line(v, r, 0, v->cols - 1);
}

/* -----------------------------------------------------------------------
 * Scrollback push
 * --------------------------------------------------------------------- */

static void sb_push_line(Vt100 *v, int row) {
  if (v->sb_cap == 0)
    return;

  int idx = (v->sb_head + v->sb_count) % v->sb_cap;
  if (v->sb_count == v->sb_cap) {
    free(v->scrollback[v->sb_head].cells);
    v->sb_head = (v->sb_head + 1) % v->sb_cap;
    v->sb_count--;
  }

  ScrollbackLine *sl = &v->scrollback[idx];
  sl->cols = v->cols;
  sl->cells = xmalloc(sizeof(VtCell) * v->cols);
  if (!sl->cells)
    return;

  VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
  memcpy(sl->cells, &screen[row * v->cols], sizeof(VtCell) * v->cols);
  v->sb_count++;
}

/* -----------------------------------------------------------------------
 * Scroll region
 * --------------------------------------------------------------------- */

static void scroll_up(Vt100 *v, int top, int bot, int n) {
  if (!v->use_alt) {
    for (int i = 0; i < n; i++)
      sb_push_line(v, top);
  }
  VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
  int lines = bot - top + 1 - n;
  if (lines > 0)
    memmove(&screen[top * v->cols], &screen[(top + n) * v->cols],
            sizeof(VtCell) * v->cols * lines);
  screen_clear_rows(v, bot - n + 1, bot);
}

static void scroll_down(Vt100 *v, int top, int bot, int n) {
  VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
  int lines = bot - top + 1 - n;
  if (lines > 0)
    memmove(&screen[(top + n) * v->cols], &screen[top * v->cols],
            sizeof(VtCell) * v->cols * lines);
  screen_clear_rows(v, top, top + n - 1);
}

/* -----------------------------------------------------------------------
 * Cursor movement helpers
 * --------------------------------------------------------------------- */

static void cursor_clamp(Vt100 *v) {
  if (v->cursor_col < 0)
    v->cursor_col = 0;
  if (v->cursor_col >= v->cols)
    v->cursor_col = v->cols - 1;
  if (v->cursor_row < 0)
    v->cursor_row = 0;
  if (v->cursor_row >= v->rows)
    v->cursor_row = v->rows - 1;
}

static void cursor_move(Vt100 *v, int col, int row) {
  v->cursor_col = col;
  v->cursor_row = row;
  v->pending_wrap = 0;
  cursor_clamp(v);
}

static void cursor_next_line(Vt100 *v) {
  if (v->cursor_row == v->scroll_bot) {
    scroll_up(v, v->scroll_top, v->scroll_bot, 1);
  } else {
    v->cursor_row++;
    if (v->cursor_row >= v->rows)
      v->cursor_row = v->rows - 1;
  }
}

/* -----------------------------------------------------------------------
 * Write a printable character at cursor
 * --------------------------------------------------------------------- */

static void put_char(Vt100 *v, uint32_t codepoint) {
  if (v->pending_wrap && v->mode_auto_wrap) {
    v->cursor_col = 0;
    v->pending_wrap = 0;
    cursor_next_line(v);
  }

  if (v->mode_insert) {
    VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
    int row = v->cursor_row;
    for (int c = v->cols - 1; c > v->cursor_col; c--)
      screen[row * v->cols + c] = screen[row * v->cols + c - 1];
  }

  VtCell *cell = screen_cell(v, v->cursor_col, v->cursor_row);
  if (cell) {
    cell->codepoint = codepoint;
    cell->fg = v->current_attr.fg;
    cell->bg = v->current_attr.bg;
    cell->attrs = v->current_attr.attrs;
  }

  if (v->cursor_col >= v->cols - 1) {
    v->pending_wrap = 1;
  } else {
    v->cursor_col++;
  }
}

/* -----------------------------------------------------------------------
 * SGR — Select Graphic Rendition
 * --------------------------------------------------------------------- */

static void apply_sgr(Vt100 *v, int *params, int count) {
  if (count == 0) {
    v->current_attr.fg = DEFAULT_FG;
    v->current_attr.bg = DEFAULT_BG;
    v->current_attr.attrs = 0;
    return;
  }

  for (int i = 0; i < count; i++) {
    int p = params[i];
    switch (p) {
    case 0:
      v->current_attr.fg = DEFAULT_FG;
      v->current_attr.bg = DEFAULT_BG;
      v->current_attr.attrs = 0;
      break;
    case 1:
      v->current_attr.attrs |= VT_ATTR_BOLD;
      break;
    case 2:
      v->current_attr.attrs |= VT_ATTR_DIM;
      break;
    case 3:
      v->current_attr.attrs |= VT_ATTR_ITALIC;
      break;
    case 4:
      v->current_attr.attrs |= VT_ATTR_UNDERLINE;
      break;
    case 5:
      v->current_attr.attrs |= VT_ATTR_BLINK;
      break;
    case 7:
      v->current_attr.attrs |= VT_ATTR_REVERSE;
      break;
    case 8:
      v->current_attr.attrs |= VT_ATTR_HIDDEN;
      break;
    case 9:
      v->current_attr.attrs |= VT_ATTR_STRIKETHROUGH;
      break;
    case 22:
      v->current_attr.attrs &= ~(VT_ATTR_BOLD | VT_ATTR_DIM);
      break;
    case 23:
      v->current_attr.attrs &= ~VT_ATTR_ITALIC;
      break;
    case 24:
      v->current_attr.attrs &= ~VT_ATTR_UNDERLINE;
      break;
    case 25:
      v->current_attr.attrs &= ~VT_ATTR_BLINK;
      break;
    case 27:
      v->current_attr.attrs &= ~VT_ATTR_REVERSE;
      break;
    case 28:
      v->current_attr.attrs &= ~VT_ATTR_HIDDEN;
      break;
    case 29:
      v->current_attr.attrs &= ~VT_ATTR_STRIKETHROUGH;
      break;
    case 39:
      v->current_attr.fg = DEFAULT_FG;
      break;
    case 49:
      v->current_attr.bg = DEFAULT_BG;
      break;
    default:
      if (p >= 30 && p <= 37) {
        v->current_attr.fg = p - 30;
        break;
      }
      if (p >= 40 && p <= 47) {
        v->current_attr.bg = p - 40;
        break;
      }
      if (p >= 90 && p <= 97) {
        v->current_attr.fg = p - 90 + 8;
        break;
      }
      if (p >= 100 && p <= 107) {
        v->current_attr.bg = p - 100 + 8;
        break;
      }
      if (p == 38 && i + 2 < count && params[i + 1] == 5) {
        v->current_attr.fg = params[i + 2];
        i += 2;
        break;
      }
      if (p == 38 && i + 4 < count && params[i + 1] == 2) {
        int r = params[i + 2], g = params[i + 3], b = params[i + 4];
        v->current_attr.fg = 16 + 36 * (r / 43) + 6 * (g / 43) + (b / 43);
        i += 4;
        break;
      }
      if (p == 48 && i + 2 < count && params[i + 1] == 5) {
        v->current_attr.bg = params[i + 2];
        i += 2;
        break;
      }
      if (p == 48 && i + 4 < count && params[i + 1] == 2) {
        int r = params[i + 2], g = params[i + 3], b = params[i + 4];
        v->current_attr.bg = 16 + 36 * (r / 43) + 6 * (g / 43) + (b / 43);
        i += 4;
        break;
      }
      break;
    }
  }
}

/* -----------------------------------------------------------------------
 * CSI dispatch
 * --------------------------------------------------------------------- */

static int param(Vt100 *v, int idx, int def) {
  if (idx >= v->param_count || v->params[idx] == 0)
    return def;
  return v->params[idx];
}

static void dispatch_csi(Vt100 *v, char final) {
  int p1 = param(v, 0, 1);
  int p2 = param(v, 1, 1);

  switch (final) {
  case 'A':
    cursor_move(v, v->cursor_col, v->cursor_row - p1);
    break;
  case 'B':
    cursor_move(v, v->cursor_col, v->cursor_row + p1);
    break;
  case 'C':
    cursor_move(v, v->cursor_col + p1, v->cursor_row);
    break;
  case 'D':
    cursor_move(v, v->cursor_col - p1, v->cursor_row);
    break;

  case 'E':
    cursor_move(v, 0, v->cursor_row + p1);
    break;
  case 'F':
    cursor_move(v, 0, v->cursor_row - p1);
    break;
  case 'G':
    cursor_move(v, p1 - 1, v->cursor_row);
    break;

  case 'H':
  case 'f':
    cursor_move(v, p2 - 1, p1 - 1);
    break;

  case 'I':
    v->cursor_col = ((v->cursor_col / 8) + p1) * 8;
    cursor_clamp(v);
    break;

  case 'J':
    switch (param(v, 0, 0)) {
    case 0:
      screen_clear_line(v, v->cursor_row, v->cursor_col, v->cols - 1);
      screen_clear_rows(v, v->cursor_row + 1, v->rows - 1);
      break;
    case 1:
      screen_clear_line(v, v->cursor_row, 0, v->cursor_col);
      screen_clear_rows(v, 0, v->cursor_row - 1);
      break;
    case 2:
      screen_clear_rows(v, 0, v->rows - 1);
      break;
    case 3:
      screen_clear_rows(v, 0, v->rows - 1);
      v->sb_count = 0;
      v->sb_head = 0;
      break;
    }
    break;

  case 'K':
    switch (param(v, 0, 0)) {
    case 0:
      screen_clear_line(v, v->cursor_row, v->cursor_col, v->cols - 1);
      break;
    case 1:
      screen_clear_line(v, v->cursor_row, 0, v->cursor_col);
      break;
    case 2:
      screen_clear_line(v, v->cursor_row, 0, v->cols - 1);
      break;
    }
    break;

  case 'L':
    scroll_down(v, v->cursor_row, v->scroll_bot, p1);
    break;

  case 'M':
    scroll_up(v, v->cursor_row, v->scroll_bot, p1);
    break;

  case 'P': {
    VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
    int row = v->cursor_row;
    int n = p1;
    if (n > v->cols - v->cursor_col)
      n = v->cols - v->cursor_col;
    int remain = v->cols - v->cursor_col - n;
    if (remain > 0)
      memmove(&screen[row * v->cols + v->cursor_col],
              &screen[row * v->cols + v->cursor_col + n],
              sizeof(VtCell) * remain);
    screen_clear_line(v, row, v->cols - n, v->cols - 1);
    break;
  }

  case '@': {
    VtCell *screen = v->use_alt ? v->alt_screen : v->screen;
    int row = v->cursor_row;
    int n = p1;
    if (n > v->cols - v->cursor_col)
      n = v->cols - v->cursor_col;
    int remain = v->cols - v->cursor_col - n;
    if (remain > 0)
      memmove(&screen[row * v->cols + v->cursor_col + n],
              &screen[row * v->cols + v->cursor_col], sizeof(VtCell) * remain);
    screen_clear_line(v, v->cursor_row, v->cursor_col, v->cursor_col + n - 1);
    break;
  }

  case 'S':
    scroll_up(v, v->scroll_top, v->scroll_bot, p1);
    break;
  case 'T':
    scroll_down(v, v->scroll_top, v->scroll_bot, p1);
    break;

  case 'X':
    screen_clear_line(v, v->cursor_row, v->cursor_col, v->cursor_col + p1 - 1);
    break;

  case 'Z':
    v->cursor_col = ((v->cursor_col - 1) / 8) * 8;
    cursor_clamp(v);
    break;

  case 'd':
    cursor_move(v, v->cursor_col, p1 - 1);
    break;

  case 'm':
    apply_sgr(v, v->params, v->param_count);
    break;

  case 'r':
    v->scroll_top = param(v, 0, 1) - 1;
    v->scroll_bot = param(v, 1, v->rows) - 1;
    if (v->scroll_top < 0)
      v->scroll_top = 0;
    if (v->scroll_bot >= v->rows)
      v->scroll_bot = v->rows - 1;
    if (v->scroll_top > v->scroll_bot) {
      v->scroll_top = 0;
      v->scroll_bot = v->rows - 1;
    }
    cursor_move(v, 0, 0);
    break;

  case 'h':
    for (int i = 0; i < v->param_count; i++) {
      if (v->interm_count > 0 && v->interm[0] == '?') {
        switch (v->params[i]) {
        case 1:
          break;
        case 6:
          v->mode_origin = 1;
          break;
        case 7:
          v->mode_auto_wrap = 1;
          break;
        case 12:
          break;
        case 25:
          v->mode_cursor_visible = 1;
          break;
        case 47:
        case 1047:
        case 1049:
          if (!v->use_alt) {
            v->alt_saved_col = v->cursor_col;
            v->alt_saved_row = v->cursor_row;
            memset(v->alt_screen, 0, sizeof(VtCell) * v->cols * v->rows);
            screen_clear_rows(v, 0, v->rows - 1);
            v->use_alt = 1;
            cursor_move(v, 0, 0);
          }
          break;
        case 2004:
          v->mode_bracketed_paste = 1;
          break;
        }
      } else {
        switch (v->params[i]) {
        case 4:
          v->mode_insert = 1;
          break;
        }
      }
    }
    break;

  case 'l':
    for (int i = 0; i < v->param_count; i++) {
      if (v->interm_count > 0 && v->interm[0] == '?') {
        switch (v->params[i]) {
        case 6:
          v->mode_origin = 0;
          break;
        case 7:
          v->mode_auto_wrap = 0;
          break;
        case 25:
          v->mode_cursor_visible = 0;
          break;
        case 47:
        case 1047:
        case 1049:
          if (v->use_alt) {
            v->use_alt = 0;
            cursor_move(v, v->alt_saved_col, v->alt_saved_row);
          }
          break;
        case 2004:
          v->mode_bracketed_paste = 0;
          break;
        }
      } else {
        switch (v->params[i]) {
        case 4:
          v->mode_insert = 0;
          break;
        }
      }
    }
    break;

  case 'n':
    break;

  case 's':
    v->saved_col = v->cursor_col;
    v->saved_row = v->cursor_row;
    break;

  case 'u':
    cursor_move(v, v->saved_col, v->saved_row);
    break;

  default:
    break;
  }
}

/* -----------------------------------------------------------------------
 * ESC dispatch
 * --------------------------------------------------------------------- */

static void dispatch_esc(Vt100 *v, char final) {
  switch (final) {
  case 'D':
    cursor_next_line(v);
    break;
  case 'E':
    v->cursor_col = 0;
    cursor_next_line(v);
    break;
  case 'M':
    if (v->cursor_row == v->scroll_top)
      scroll_down(v, v->scroll_top, v->scroll_bot, 1);
    else if (v->cursor_row > 0)
      v->cursor_row--;
    break;
  case '7':
    v->saved_col = v->cursor_col;
    v->saved_row = v->cursor_row;
    break;
  case '8':
    cursor_move(v, v->saved_col, v->saved_row);
    break;
  case 'c':
    screen_clear_rows(v, 0, v->rows - 1);
    cursor_move(v, 0, 0);
    v->current_attr.fg = DEFAULT_FG;
    v->current_attr.bg = DEFAULT_BG;
    v->current_attr.attrs = 0;
    v->scroll_top = 0;
    v->scroll_bot = v->rows - 1;
    break;
  default:
    break;
  }
}

/* -----------------------------------------------------------------------
 * OSC dispatch
 * --------------------------------------------------------------------- */

static void dispatch_osc(Vt100 *v) { (void)v; }

/* -----------------------------------------------------------------------
 * Parser — VT state machine
 * --------------------------------------------------------------------- */

static void parser_reset(Vt100 *v) {
  v->param_count = 0;
  v->current_param = 0;
  v->interm_count = 0;
  memset(v->params, 0, sizeof(v->params));
  memset(v->interm, 0, sizeof(v->interm));
}

static void param_push_digit(Vt100 *v, char c) {
  if (v->param_count == 0)
    v->param_count = 1;
  v->params[v->param_count - 1] =
      v->params[v->param_count - 1] * 10 + (c - '0');
}

static void param_next(Vt100 *v) {
  if (v->param_count < MAX_PARAMS)
    v->param_count++;
}

void vt100_feed(Vt100 *v, const char *buf, int len) {
  for (int i = 0; i < len; i++) {
    unsigned char c = (unsigned char)buf[i];

    if (v->parse_state == VT_STATE_OSC) {
      if (c == 0x07 || c == 0x9C ||
          (c == '\\' && v->osc_len > 0 && v->osc_buf[v->osc_len - 1] == 0x1B)) {
        if (v->osc_len > 0 && v->osc_buf[v->osc_len - 1] == 0x1B)
          v->osc_len--;
        v->osc_buf[v->osc_len] = '\0';
        dispatch_osc(v);
        v->parse_state = VT_STATE_GROUND;
      } else {
        if (v->osc_len < MAX_OSC_LEN - 1)
          v->osc_buf[v->osc_len++] = (char)c;
      }
      continue;
    }

    switch (v->parse_state) {
    case VT_STATE_GROUND:
      if (c < 0x20) {
        switch (c) {
        case 0x07:
          break;
        case 0x08:
          if (v->cursor_col > 0)
            v->cursor_col--;
          v->pending_wrap = 0;
          break;
        case 0x09:
          v->cursor_col = ((v->cursor_col / 8) + 1) * 8;
          if (v->cursor_col >= v->cols)
            v->cursor_col = v->cols - 1;
          v->pending_wrap = 0;
          break;
        case 0x0A:
        case 0x0B:
        case 0x0C:
          cursor_next_line(v);
          break;
        case 0x0D:
          v->cursor_col = 0;
          v->pending_wrap = 0;
          break;
        case 0x0E:
        case 0x0F:
          break;
        case 0x1B:
          parser_reset(v);
          v->parse_state = VT_STATE_ESCAPE;
          break;
        default:
          break;
        }
      } else {
        put_char(v, (uint32_t)c);
      }
      break;

    case VT_STATE_ESCAPE:
      if (c == '[') {
        parser_reset(v);
        v->parse_state = VT_STATE_CSI;
      } else if (c == ']') {
        v->osc_len = 0;
        v->parse_state = VT_STATE_OSC;
      } else if (c == 'P' || c == 'X' || c == '^' || c == '_') {
        v->parse_state = VT_STATE_OSC;
        v->osc_len = 0;
      } else if (c >= 0x20 && c <= 0x2F) {
        if (v->interm_count < MAX_INTERM)
          v->interm[v->interm_count++] = (char)c;
      } else if (c >= 0x30 && c <= 0x7E) {
        dispatch_esc(v, (char)c);
        v->parse_state = VT_STATE_GROUND;
      } else {
        v->parse_state = VT_STATE_GROUND;
      }
      break;

    case VT_STATE_CSI:
      if (c >= '0' && c <= '9') {
        param_push_digit(v, (char)c);
      } else if (c == ';') {
        param_next(v);
      } else if (c == ':') {
        param_next(v);
      } else if (c >= 0x20 && c <= 0x2F) {
        if (v->interm_count < MAX_INTERM)
          v->interm[v->interm_count++] = (char)c;
      } else if (c >= 0x40 && c <= 0x7E) {
        dispatch_csi(v, (char)c);
        v->parse_state = VT_STATE_GROUND;
      } else if (c == 0x1B) {
        parser_reset(v);
        v->parse_state = VT_STATE_ESCAPE;
      } else {
        v->parse_state = VT_STATE_GROUND;
      }
      break;

    default:
      v->parse_state = VT_STATE_GROUND;
      break;
    }
  }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

Vt100 *vt100_new(int cols, int rows, int scrollback) {
  if (cols <= 0 || rows <= 0)
    return NULL;

  Vt100 *v = xmalloc(sizeof(Vt100));
  if (!v)
    return NULL;
  memset(v, 0, sizeof(Vt100));

  v->cols = cols;
  v->rows = rows;

  v->screen = xmalloc(sizeof(VtCell) * cols * rows);
  if (!v->screen) {
    free(v);
    return NULL;
  }

  v->alt_screen = xmalloc(sizeof(VtCell) * cols * rows);
  if (!v->alt_screen) {
    free(v->screen);
    free(v);
    return NULL;
  }

  v->current_attr.fg = DEFAULT_FG;
  v->current_attr.bg = DEFAULT_BG;
  v->current_attr.attrs = 0;

  v->scroll_top = 0;
  v->scroll_bot = rows - 1;
  v->mode_auto_wrap = 1;
  v->mode_cursor_visible = 1;
  v->parse_state = VT_STATE_GROUND;

  screen_clear_rows(v, 0, rows - 1);

  if (scrollback > 0) {
    v->scrollback = xmalloc(sizeof(ScrollbackLine) * scrollback);
    if (v->scrollback) {
      memset(v->scrollback, 0, sizeof(ScrollbackLine) * scrollback);
      v->sb_cap = scrollback;
    }
  }

  return v;
}

void vt100_free(Vt100 *v) {
  if (!v)
    return;
  free(v->screen);
  free(v->alt_screen);
  if (v->scrollback) {
    for (int i = 0; i < v->sb_count; i++) {
      int idx = (v->sb_head + i) % v->sb_cap;
      free(v->scrollback[idx].cells);
    }
    free(v->scrollback);
  }
  free(v);
}

void vt100_resize(Vt100 *v, int cols, int rows) {
  if (!v || cols <= 0 || rows <= 0)
    return;

  VtCell *new_screen = xmalloc(sizeof(VtCell) * cols * rows);
  VtCell *new_alt = xmalloc(sizeof(VtCell) * cols * rows);
  if (!new_screen || !new_alt) {
    free(new_screen);
    free(new_alt);
    return;
  }

  VtCell blank = cell_blank();
  for (int i = 0; i < cols * rows; i++) {
    new_screen[i] = blank;
    new_alt[i] = blank;
  }

  int copy_rows = rows < v->rows ? rows : v->rows;
  int copy_cols = cols < v->cols ? cols : v->cols;
  for (int r = 0; r < copy_rows; r++)
    for (int c = 0; c < copy_cols; c++)
      new_screen[r * cols + c] = v->screen[r * v->cols + c];

  free(v->screen);
  free(v->alt_screen);
  v->screen = new_screen;
  v->alt_screen = new_alt;
  v->cols = cols;
  v->rows = rows;

  if (v->cursor_col >= cols)
    v->cursor_col = cols - 1;
  if (v->cursor_row >= rows)
    v->cursor_row = rows - 1;
  v->scroll_top = 0;
  v->scroll_bot = rows - 1;
}

VtCell *vt100_cell(Vt100 *v, int col, int row) {
  return screen_cell(v, col, row);
}

int vt100_cursor_col(Vt100 *v) { return v ? v->cursor_col : 0; }
int vt100_cursor_row(Vt100 *v) { return v ? v->cursor_row : 0; }
int vt100_cursor_visible(Vt100 *v) { return v ? v->mode_cursor_visible : 1; }
int vt100_cols(Vt100 *v) { return v ? v->cols : 0; }
int vt100_rows(Vt100 *v) { return v ? v->rows : 0; }

VtCell *vt100_scrollback_line(Vt100 *v, int index, int *out_cols) {
  if (!v || index < 0 || index >= v->sb_count)
    return NULL;
  int idx = (v->sb_head + v->sb_count - 1 - index) % v->sb_cap;
  if (out_cols)
    *out_cols = v->scrollback[idx].cols;
  return v->scrollback[idx].cells;
}

int vt100_scrollback_count(Vt100 *v) { return v ? v->sb_count : 0; }