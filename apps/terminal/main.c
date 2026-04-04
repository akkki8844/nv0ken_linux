#include "vt100.h"
#include "pty.h"
#include "../../../libnv0/include/nv0/app.h"
#include "../../../libnv0/include/nv0/window.h"
#include "../../../libnv0/include/nv0/input.h"
#include "../../../libnv0/include/nv0/draw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WINDOW_W        800
#define WINDOW_H        500
#define CELL_W          8
#define CELL_H          16
#define PADDING         4
#define COLS            ((WINDOW_W - PADDING * 2) / CELL_W)
#define ROWS            ((WINDOW_H - PADDING * 2) / CELL_H)
#define SCROLLBACK      1000
#define BLINK_MS        500

#define COL_BG          NV_COLOR(0x1E, 0x1E, 0x1E, 0xFF)
#define COL_FG          NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_CURSOR      NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_SELECT      NV_COLOR(0x26, 0x4F, 0x78, 0xFF)

static const NvColor ANSI_COLORS[16] = {
    NV_COLOR(0x1E, 0x1E, 0x1E, 0xFF),
    NV_COLOR(0xCD, 0x31, 0x31, 0xFF),
    NV_COLOR(0x0D, 0xBC, 0x79, 0xFF),
    NV_COLOR(0xE5, 0xC0, 0x7B, 0xFF),
    NV_COLOR(0x52, 0x9B, 0xD8, 0xFF),
    NV_COLOR(0xC6, 0x78, 0xDD, 0xFF),
    NV_COLOR(0x56, 0xB6, 0xC2, 0xFF),
    NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF),
    NV_COLOR(0x80, 0x80, 0x80, 0xFF),
    NV_COLOR(0xF1, 0x45, 0x14, 0xFF),
    NV_COLOR(0x23, 0xD1, 0x8B, 0xFF),
    NV_COLOR(0xF5, 0xF5, 0x43, 0xFF),
    NV_COLOR(0x3B, 0x8E, 0xEA, 0xFF),
    NV_COLOR(0xD6, 0x70, 0xDE, 0xFF),
    NV_COLOR(0x29, 0xB8, 0xDB, 0xFF),
    NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF),
};

typedef struct {
    NvWindow  *window;
    NvSurface *surface;

    Vt100     *vt;
    Pty       *pty;

    int        blink_on;
    int        blink_ticks;

    int        sel_active;
    int        sel_start_col;
    int        sel_start_row;
    int        sel_end_col;
    int        sel_end_row;
    int        sel_dragging;
} Terminal;

static Terminal g_term;

/* -----------------------------------------------------------------------
 * Color resolution
 * --------------------------------------------------------------------- */

static NvColor resolve_color(int index, int is_bg) {
    if (index == VT_COLOR_DEFAULT) return is_bg ? COL_BG : COL_FG;
    if (index >= 0 && index < 16)  return ANSI_COLORS[index];
    if (index >= 16 && index < 232) {
        int i  = index - 16;
        int b  = i % 6; i /= 6;
        int g  = i % 6; i /= 6;
        int r  = i % 6;
        uint8_t rv = r ? (uint8_t)(r * 40 + 55) : 0;
        uint8_t gv = g ? (uint8_t)(g * 40 + 55) : 0;
        uint8_t bv = b ? (uint8_t)(b * 40 + 55) : 0;
        return NV_COLOR(rv, gv, bv, 0xFF);
    }
    if (index >= 232 && index < 256) {
        uint8_t v = (uint8_t)((index - 232) * 10 + 8);
        return NV_COLOR(v, v, v, 0xFF);
    }
    return is_bg ? COL_BG : COL_FG;
}

/* -----------------------------------------------------------------------
 * Selection helpers
 * --------------------------------------------------------------------- */

static int sel_contains(Terminal *t, int col, int row) {
    if (!t->sel_active) return 0;
    int r0 = t->sel_start_row, c0 = t->sel_start_col;
    int r1 = t->sel_end_row,   c1 = t->sel_end_col;
    if (r0 > r1 || (r0 == r1 && c0 > c1)) {
        int tmp; tmp = r0; r0 = r1; r1 = tmp;
                 tmp = c0; c0 = c1; c1 = tmp;
    }
    if (row < r0 || row > r1) return 0;
    if (row == r0 && col < c0) return 0;
    if (row == r1 && col > c1) return 0;
    return 1;
}

static char *sel_copy_text(Terminal *t) {
    if (!t->sel_active) return NULL;
    int r0 = t->sel_start_row, c0 = t->sel_start_col;
    int r1 = t->sel_end_row,   c1 = t->sel_end_col;
    if (r0 > r1 || (r0 == r1 && c0 > c1)) {
        int tmp; tmp = r0; r0 = r1; r1 = tmp;
                 tmp = c0; c0 = c1; c1 = tmp;
    }
    char *buf = malloc(COLS * (r1 - r0 + 1) * 4 + 8);
    if (!buf) return NULL;
    int pos = 0;
    for (int row = r0; row <= r1; row++) {
        int col_start = (row == r0) ? c0 : 0;
        int col_end   = (row == r1) ? c1 : COLS - 1;
        for (int col = col_start; col <= col_end; col++) {
            VtCell *cell = vt100_cell(t->vt, col, row);
            if (cell && cell->codepoint >= 32)
                buf[pos++] = (char)(cell->codepoint & 0x7F);
            else
                buf[pos++] = ' ';
        }
        if (row < r1) buf[pos++] = '\n';
    }
    buf[pos] = '\0';
    return buf;
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_cell(Terminal *t, int col, int row, VtCell *cell, int cursor) {
    int x = PADDING + col * CELL_W;
    int y = PADDING + row * CELL_H;

    NvColor bg = resolve_color(cell ? cell->bg : VT_COLOR_DEFAULT, 1);
    NvColor fg = resolve_color(cell ? cell->fg : VT_COLOR_DEFAULT, 0);

    if (cell && cell->attrs & VT_ATTR_REVERSE) {
        NvColor tmp = bg; bg = fg; fg = tmp;
    }

    if (sel_contains(t, col, row)) bg = COL_SELECT;

    NvRect cell_rect = { x, y, CELL_W, CELL_H };
    nv_draw_fill_rect(t->surface, &cell_rect, bg);

    if (cursor && t->blink_on) {
        NvRect cur_rect = { x, y + CELL_H - 2, CELL_W, 2 };
        nv_draw_fill_rect(t->surface, &cur_rect, COL_CURSOR);
    }

    if (!cell || cell->codepoint < 32) return;

    char glyph[2] = { (char)(cell->codepoint & 0xFF), '\0' };
    int bold      = (cell->attrs & VT_ATTR_BOLD) ? 1 : 0;
    int underline = (cell->attrs & VT_ATTR_UNDERLINE) ? 1 : 0;

    nv_draw_text_styled(t->surface, x, y, glyph, fg, CELL_H, bold, 0);

    if (underline) {
        NvRect ul = { x, y + CELL_H - 1, CELL_W, 1 };
        nv_draw_fill_rect(t->surface, &ul, fg);
    }
}

static void on_paint(NvWindow *win, NvSurface *surface, void *userdata) {
    (void)win;
    Terminal *t = (Terminal *)userdata;
    t->surface = surface;

    NvRect bg = { 0, 0, WINDOW_W, WINDOW_H };
    nv_draw_fill_rect(surface, &bg, COL_BG);

    int cur_col = vt100_cursor_col(t->vt);
    int cur_row = vt100_cursor_row(t->vt);

    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {
            VtCell *cell = vt100_cell(t->vt, col, row);
            int is_cursor = (col == cur_col && row == cur_row);
            draw_cell(t, col, row, cell, is_cursor);
        }
    }
}

/* -----------------------------------------------------------------------
 * PTY read callback — called when shell writes output
 * --------------------------------------------------------------------- */

static void on_pty_data(const char *buf, int len, void *userdata) {
    Terminal *t = (Terminal *)userdata;
    vt100_feed(t->vt, buf, len);
    nv_surface_invalidate(t->surface);
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

static void on_key_down(NvWindow *win, NvKeyEvent *ev, void *userdata) {
    (void)win;
    Terminal *t = (Terminal *)userdata;

    if (ev->mod & NV_MOD_CTRL && ev->mod & NV_MOD_SHIFT) {
        if (ev->key == 'C' || ev->key == 'c') {
            char *text = sel_copy_text(t);
            if (text) { nv_clipboard_set(text); free(text); }
            return;
        }
        if (ev->key == 'V' || ev->key == 'v') {
            char *text = nv_clipboard_get();
            if (text) { pty_write(t->pty, text, strlen(text)); free(text); }
            return;
        }
    }

    char seq[16] = {0};
    int  seq_len = 0;

    switch (ev->key) {
        case NV_KEY_RETURN:    seq[0] = '\r'; seq_len = 1; break;
        case NV_KEY_BACKSPACE: seq[0] = 0x7F; seq_len = 1; break;
        case NV_KEY_TAB:       seq[0] = '\t'; seq_len = 1; break;
        case NV_KEY_ESCAPE:    seq[0] = 0x1B; seq_len = 1; break;

        case NV_KEY_UP:    memcpy(seq, "\x1B[A", 3); seq_len = 3; break;
        case NV_KEY_DOWN:  memcpy(seq, "\x1B[B", 3); seq_len = 3; break;
        case NV_KEY_RIGHT: memcpy(seq, "\x1B[C", 3); seq_len = 3; break;
        case NV_KEY_LEFT:  memcpy(seq, "\x1B[D", 3); seq_len = 3; break;

        case NV_KEY_HOME:   memcpy(seq, "\x1B[H",   3); seq_len = 3; break;
        case NV_KEY_END:    memcpy(seq, "\x1B[F",   3); seq_len = 3; break;
        case NV_KEY_PAGE_UP:  memcpy(seq, "\x1B[5~", 4); seq_len = 4; break;
        case NV_KEY_PAGE_DOWN:memcpy(seq, "\x1B[6~", 4); seq_len = 4; break;
        case NV_KEY_INSERT:   memcpy(seq, "\x1B[2~", 4); seq_len = 4; break;
        case NV_KEY_DELETE:   memcpy(seq, "\x1B[3~", 4); seq_len = 4; break;

        case NV_KEY_F1:  memcpy(seq, "\x1BOP",   3); seq_len = 3; break;
        case NV_KEY_F2:  memcpy(seq, "\x1BOQ",   3); seq_len = 3; break;
        case NV_KEY_F3:  memcpy(seq, "\x1BOR",   3); seq_len = 3; break;
        case NV_KEY_F4:  memcpy(seq, "\x1BOS",   3); seq_len = 3; break;
        case NV_KEY_F5:  memcpy(seq, "\x1B[15~", 5); seq_len = 5; break;
        case NV_KEY_F6:  memcpy(seq, "\x1B[17~", 5); seq_len = 5; break;
        case NV_KEY_F7:  memcpy(seq, "\x1B[18~", 5); seq_len = 5; break;
        case NV_KEY_F8:  memcpy(seq, "\x1B[19~", 5); seq_len = 5; break;
        case NV_KEY_F9:  memcpy(seq, "\x1B[20~", 5); seq_len = 5; break;
        case NV_KEY_F10: memcpy(seq, "\x1B[21~", 5); seq_len = 5; break;
        case NV_KEY_F11: memcpy(seq, "\x1B[23~", 5); seq_len = 5; break;
        case NV_KEY_F12: memcpy(seq, "\x1B[24~", 5); seq_len = 5; break;

        default:
            if (ev->mod & NV_MOD_CTRL && ev->codepoint >= 'a' && ev->codepoint <= 'z') {
                seq[0] = (char)(ev->codepoint - 'a' + 1);
                seq_len = 1;
            } else if (ev->mod & NV_MOD_CTRL && ev->codepoint >= 'A' && ev->codepoint <= 'Z') {
                seq[0] = (char)(ev->codepoint - 'A' + 1);
                seq_len = 1;
            } else if (ev->codepoint >= 32 && ev->codepoint < 127) {
                seq[0] = (char)ev->codepoint;
                seq_len = 1;
            }
            break;
    }

    if (seq_len > 0) pty_write(t->pty, seq, seq_len);
}

static void on_mouse_down(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    Terminal *t = (Terminal *)userdata;
    if (ev->button != NV_MOUSE_LEFT) return;

    int col = (ev->x - PADDING) / CELL_W;
    int row = (ev->y - PADDING) / CELL_H;
    if (col < 0) col = 0; if (col >= COLS) col = COLS - 1;
    if (row < 0) row = 0; if (row >= ROWS) row = ROWS - 1;

    t->sel_active    = 1;
    t->sel_dragging  = 1;
    t->sel_start_col = col;
    t->sel_start_row = row;
    t->sel_end_col   = col;
    t->sel_end_row   = row;
    nv_surface_invalidate(t->surface);
}

static void on_mouse_up(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win; (void)ev;
    Terminal *t = (Terminal *)userdata;
    t->sel_dragging = 0;
}

static void on_mouse_move(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    Terminal *t = (Terminal *)userdata;
    if (!t->sel_dragging) return;

    int col = (ev->x - PADDING) / CELL_W;
    int row = (ev->y - PADDING) / CELL_H;
    if (col < 0) col = 0; if (col >= COLS) col = COLS - 1;
    if (row < 0) row = 0; if (row >= ROWS) row = ROWS - 1;

    t->sel_end_col = col;
    t->sel_end_row = row;
    nv_surface_invalidate(t->surface);
}

static void on_timer(void *userdata) {
    Terminal *t = (Terminal *)userdata;
    t->blink_ticks++;
    if (t->blink_ticks >= BLINK_MS / 16) {
        t->blink_ticks = 0;
        t->blink_on    = !t->blink_on;
        nv_surface_invalidate(t->surface);
    }
    int n = pty_poll(t->pty);
    if (n > 0) nv_surface_invalidate(t->surface);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    memset(&g_term, 0, sizeof(Terminal));
    g_term.blink_on = 1;

    g_term.vt = vt100_new(COLS, ROWS, SCROLLBACK);
    if (!g_term.vt) return 1;

    g_term.pty = pty_open(COLS, ROWS, on_pty_data, &g_term);
    if (!g_term.pty) { vt100_free(g_term.vt); return 1; }

    NvWindowConfig cfg;
    cfg.title    = "Terminal";
    cfg.width    = WINDOW_W;
    cfg.height   = WINDOW_H;
    cfg.resizable = 0;

    g_term.window = nv_window_create(&cfg);
    if (!g_term.window) return 1;

    nv_window_on_paint(g_term.window,      on_paint,     &g_term);
    nv_window_on_key_down(g_term.window,   on_key_down,  &g_term);
    nv_window_on_mouse_down(g_term.window, on_mouse_down,&g_term);
    nv_window_on_mouse_up(g_term.window,   on_mouse_up,  &g_term);
    nv_window_on_mouse_move(g_term.window, on_mouse_move,&g_term);
    nv_timer_set(16, on_timer, &g_term);

    return nv_app_run();
}