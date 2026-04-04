#include "../../../libnv0/include/nv0/app.h"
#include "../../../libnv0/include/nv0/window.h"
#include "../../../libnv0/include/nv0/input.h"
#include "../../../libnv0/include/nv0/draw.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define WINDOW_W     320
#define WINDOW_H     480
#define DISPLAY_H    100
#define BTN_COLS     4
#define BTN_ROWS     6
#define BTN_W        (WINDOW_W / BTN_COLS)
#define BTN_H        ((WINDOW_H - DISPLAY_H) / BTN_ROWS)
#define MAX_EXPR     128
#define MAX_HISTORY  16

typedef enum {
    OP_NONE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_POW,
} Operator;

typedef enum {
    BTN_DIGIT,
    BTN_OP,
    BTN_EQUALS,
    BTN_CLEAR,
    BTN_CLEAR_ENTRY,
    BTN_BACKSPACE,
    BTN_DECIMAL,
    BTN_NEGATE,
    BTN_PERCENT,
    BTN_SQRT,
    BTN_SQUARE,
    BTN_RECIPROCAL,
    BTN_HISTORY,
} BtnType;

typedef struct {
    BtnType   type;
    char      label[8];
    int       value;
    NvColor   bg;
    NvColor   fg;
} Button;

typedef struct {
    double  operand;
    double  result;
    Operator op;
    char    expression[MAX_EXPR];
} HistoryEntry;

typedef struct {
    char    display[MAX_EXPR];
    char    expr_line[MAX_EXPR];

    double  operand_a;
    double  operand_b;
    Operator pending_op;

    int     has_result;
    int     decimal_entered;
    int     fresh_operand;
    int     error;

    HistoryEntry history[MAX_HISTORY];
    int          history_count;
    int          show_history;

    NvWindow  *window;
    NvSurface *surface;
} Calculator;

static Calculator g_calc;

static NvColor COL_DISPLAY_BG  = NV_COLOR(0x1E, 0x1E, 0x1E, 0xFF);
static NvColor COL_DISPLAY_FG  = NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF);
static NvColor COL_EXPR_FG     = NV_COLOR(0xAA, 0xAA, 0xAA, 0xFF);
static NvColor COL_BTN_NUM     = NV_COLOR(0x30, 0x30, 0x30, 0xFF);
static NvColor COL_BTN_OP      = NV_COLOR(0xFF, 0x9F, 0x0A, 0xFF);
static NvColor COL_BTN_FUNC    = NV_COLOR(0x3A, 0x3A, 0x3C, 0xFF);
static NvColor COL_BTN_CLEAR   = NV_COLOR(0xFF, 0x45, 0x3A, 0xFF);
static NvColor COL_BTN_EQUALS  = NV_COLOR(0x30, 0xD1, 0x58, 0xFF);
static NvColor COL_BTN_FG      = NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF);
static NvColor COL_BTN_BORDER  = NV_COLOR(0x1E, 0x1E, 0x1E, 0xFF);
static NvColor COL_HISTORY_BG  = NV_COLOR(0x28, 0x28, 0x28, 0xFF);
static NvColor COL_HISTORY_FG  = NV_COLOR(0xDD, 0xDD, 0xDD, 0xFF);
static NvColor COL_HISTORY_SEP = NV_COLOR(0x44, 0x44, 0x44, 0xFF);

static const Button BUTTONS[BTN_ROWS][BTN_COLS] = {
    {
        { BTN_CLEAR,      "AC",   0, {0},{0} },
        { BTN_NEGATE,     "+/-",  0, {0},{0} },
        { BTN_PERCENT,    "%",    0, {0},{0} },
        { BTN_OP,         "/",  OP_DIV, {0},{0} },
    },
    {
        { BTN_DIGIT,      "7",    7, {0},{0} },
        { BTN_DIGIT,      "8",    8, {0},{0} },
        { BTN_DIGIT,      "9",    9, {0},{0} },
        { BTN_OP,         "x",  OP_MUL, {0},{0} },
    },
    {
        { BTN_DIGIT,      "4",    4, {0},{0} },
        { BTN_DIGIT,      "5",    5, {0},{0} },
        { BTN_DIGIT,      "6",    6, {0},{0} },
        { BTN_OP,         "-",  OP_SUB, {0},{0} },
    },
    {
        { BTN_DIGIT,      "1",    1, {0},{0} },
        { BTN_DIGIT,      "2",    2, {0},{0} },
        { BTN_DIGIT,      "3",    3, {0},{0} },
        { BTN_OP,         "+",  OP_ADD, {0},{0} },
    },
    {
        { BTN_SQRT,       "sqrt", 0, {0},{0} },
        { BTN_DIGIT,      "0",    0, {0},{0} },
        { BTN_DECIMAL,    ".",    0, {0},{0} },
        { BTN_EQUALS,     "=",    0, {0},{0} },
    },
    {
        { BTN_SQUARE,     "x^2",  0, {0},{0} },
        { BTN_RECIPROCAL, "1/x",  0, {0},{0} },
        { BTN_BACKSPACE,  "del",  0, {0},{0} },
        { BTN_HISTORY,    "hist", 0, {0},{0} },
    },
};

static NvColor btn_color(const Button *b) {
    switch (b->type) {
        case BTN_CLEAR:
        case BTN_CLEAR_ENTRY:  return COL_BTN_CLEAR;
        case BTN_EQUALS:       return COL_BTN_EQUALS;
        case BTN_OP:           return COL_BTN_OP;
        case BTN_DIGIT:
        case BTN_DECIMAL:      return COL_BTN_NUM;
        default:               return COL_BTN_FUNC;
    }
}

static void calc_reset(Calculator *c) {
    memset(c->display, 0, sizeof(c->display));
    memset(c->expr_line, 0, sizeof(c->expr_line));
    strcpy(c->display, "0");
    c->operand_a       = 0.0;
    c->operand_b       = 0.0;
    c->pending_op      = OP_NONE;
    c->has_result      = 0;
    c->decimal_entered = 0;
    c->fresh_operand   = 1;
    c->error           = 0;
}

static double get_display_value(Calculator *c) {
    return atof(c->display);
}

static void set_display_double(Calculator *c, double v) {
    if (v == (long long)v && fabs(v) < 1e15) {
        snprintf(c->display, MAX_EXPR, "%lld", (long long)v);
    } else {
        snprintf(c->display, MAX_EXPR, "%.10g", v);
    }
    c->decimal_entered = strchr(c->display, '.') != NULL;
}

static void push_history(Calculator *c, double a, double b, Operator op, double result) {
    if (c->history_count >= MAX_HISTORY) {
        memmove(&c->history[0], &c->history[1],
                sizeof(HistoryEntry) * (MAX_HISTORY - 1));
        c->history_count--;
    }
    HistoryEntry *e = &c->history[c->history_count++];
    e->operand = a;
    e->result  = result;
    e->op      = op;

    char op_ch = '?';
    switch (op) {
        case OP_ADD: op_ch = '+'; break;
        case OP_SUB: op_ch = '-'; break;
        case OP_MUL: op_ch = 'x'; break;
        case OP_DIV: op_ch = '/'; break;
        case OP_MOD: op_ch = '%'; break;
        case OP_POW: op_ch = '^'; break;
        default: op_ch = '?'; break;
    }

    char a_str[32], b_str[32], r_str[32];
    snprintf(a_str, sizeof(a_str), "%.10g", a);
    snprintf(b_str, sizeof(b_str), "%.10g", b);
    snprintf(r_str, sizeof(r_str), "%.10g", result);
    snprintf(e->expression, MAX_EXPR, "%s %c %s = %s", a_str, op_ch, b_str, r_str);
}

static double apply_op(Operator op, double a, double b, int *err) {
    switch (op) {
        case OP_ADD: return a + b;
        case OP_SUB: return a - b;
        case OP_MUL: return a * b;
        case OP_DIV:
            if (b == 0.0) { *err = 1; return 0.0; }
            return a / b;
        case OP_MOD:
            if (b == 0.0) { *err = 1; return 0.0; }
            return fmod(a, b);
        case OP_POW: return pow(a, b);
        default:     return b;
    }
}

static void calc_evaluate(Calculator *c) {
    if (c->pending_op == OP_NONE) return;

    double b = get_display_value(c);
    int err  = 0;
    double result = apply_op(c->pending_op, c->operand_a, b, &err);

    if (err) {
        strcpy(c->display, "Error");
        strcpy(c->expr_line, "Division by zero");
        c->error       = 1;
        c->pending_op  = OP_NONE;
        c->fresh_operand = 1;
        nv_surface_invalidate(c->surface);
        return;
    }

    push_history(c, c->operand_a, b, c->pending_op, result);

    char op_ch = '+';
    switch (c->pending_op) {
        case OP_ADD: op_ch = '+'; break;
        case OP_SUB: op_ch = '-'; break;
        case OP_MUL: op_ch = 'x'; break;
        case OP_DIV: op_ch = '/'; break;
        case OP_MOD: op_ch = '%'; break;
        case OP_POW: op_ch = '^'; break;
        default: break;
    }

    char a_str[32], b_str[32];
    snprintf(a_str, sizeof(a_str), "%.10g", c->operand_a);
    snprintf(b_str, sizeof(b_str), "%.10g", b);
    snprintf(c->expr_line, MAX_EXPR, "%s %c %s =", a_str, op_ch, b_str);

    set_display_double(c, result);
    c->operand_a       = result;
    c->pending_op      = OP_NONE;
    c->has_result      = 1;
    c->fresh_operand   = 1;
    c->decimal_entered = 0;
}

static void on_btn_digit(Calculator *c, int digit) {
    if (c->error) calc_reset(c);
    if (c->fresh_operand) {
        strcpy(c->display, "0");
        c->decimal_entered = 0;
        c->fresh_operand   = 0;
    }
    if (c->has_result) {
        c->has_result  = 0;
        c->pending_op  = OP_NONE;
        strcpy(c->display, "0");
        c->decimal_entered = 0;
    }

    if (strcmp(c->display, "0") == 0 && digit == 0) return;
    if (strcmp(c->display, "0") == 0 && digit != 0) {
        snprintf(c->display, MAX_EXPR, "%d", digit);
        return;
    }

    int len = strlen(c->display);
    if (len >= MAX_EXPR - 2) return;
    c->display[len]     = '0' + digit;
    c->display[len + 1] = '\0';
}

static void on_btn_decimal(Calculator *c) {
    if (c->error) calc_reset(c);
    if (c->decimal_entered) return;
    if (c->fresh_operand) {
        strcpy(c->display, "0");
        c->fresh_operand = 0;
    }
    int len = strlen(c->display);
    if (len >= MAX_EXPR - 2) return;
    c->display[len]     = '.';
    c->display[len + 1] = '\0';
    c->decimal_entered  = 1;
}

static void on_btn_op(Calculator *c, Operator op) {
    if (c->error) calc_reset(c);
    if (c->pending_op != OP_NONE && !c->fresh_operand)
        calc_evaluate(c);

    c->operand_a     = get_display_value(c);
    c->pending_op    = op;
    c->fresh_operand = 1;
    c->has_result    = 0;

    char op_ch = '+';
    switch (op) {
        case OP_ADD: op_ch = '+'; break;
        case OP_SUB: op_ch = '-'; break;
        case OP_MUL: op_ch = 'x'; break;
        case OP_DIV: op_ch = '/'; break;
        case OP_MOD: op_ch = '%'; break;
        case OP_POW: op_ch = '^'; break;
        default: break;
    }
    snprintf(c->expr_line, MAX_EXPR, "%.10g %c", c->operand_a, op_ch);
}

static void on_btn_clear(Calculator *c) {
    calc_reset(c);
}

static void on_btn_backspace(Calculator *c) {
    if (c->error) { calc_reset(c); return; }
    if (c->fresh_operand || c->has_result) return;
    int len = strlen(c->display);
    if (len <= 1) {
        strcpy(c->display, "0");
        c->decimal_entered = 0;
        return;
    }
    if (c->display[len - 1] == '.') c->decimal_entered = 0;
    c->display[len - 1] = '\0';
}

static void on_btn_negate(Calculator *c) {
    if (c->error) return;
    double v = get_display_value(c);
    set_display_double(c, -v);
}

static void on_btn_percent(Calculator *c) {
    if (c->error) return;
    double v = get_display_value(c);
    if (c->pending_op != OP_NONE)
        set_display_double(c, c->operand_a * (v / 100.0));
    else
        set_display_double(c, v / 100.0);
    c->decimal_entered = strchr(c->display, '.') != NULL;
}

static void on_btn_sqrt(Calculator *c) {
    if (c->error) return;
    double v = get_display_value(c);
    if (v < 0.0) {
        strcpy(c->display, "Error");
        strcpy(c->expr_line, "sqrt of negative");
        c->error = 1;
        nv_surface_invalidate(c->surface);
        return;
    }
    char expr[64];
    snprintf(expr, sizeof(expr), "sqrt(%.10g) =", v);
    strcpy(c->expr_line, expr);
    set_display_double(c, sqrt(v));
    c->fresh_operand = 1;
}

static void on_btn_square(Calculator *c) {
    if (c->error) return;
    double v = get_display_value(c);
    char expr[64];
    snprintf(expr, sizeof(expr), "(%.10g)^2 =", v);
    strcpy(c->expr_line, expr);
    set_display_double(c, v * v);
    c->fresh_operand = 1;
}

static void on_btn_reciprocal(Calculator *c) {
    if (c->error) return;
    double v = get_display_value(c);
    if (v == 0.0) {
        strcpy(c->display, "Error");
        strcpy(c->expr_line, "1/0 undefined");
        c->error = 1;
        nv_surface_invalidate(c->surface);
        return;
    }
    char expr[64];
    snprintf(expr, sizeof(expr), "1/(%.10g) =", v);
    strcpy(c->expr_line, expr);
    set_display_double(c, 1.0 / v);
    c->fresh_operand = 1;
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_display(Calculator *c) {
    NvRect bg = {0, 0, WINDOW_W, DISPLAY_H};
    nv_draw_fill_rect(c->surface, &bg, COL_DISPLAY_BG);

    if (c->expr_line[0]) {
        nv_draw_text(c->surface, 12, 18, c->expr_line, COL_EXPR_FG);
    }

    const char *disp = c->display[0] ? c->display : "0";
    int font_size = 32;
    int text_w    = (int)strlen(disp) * (font_size * 6 / 10);
    while (text_w > WINDOW_W - 24 && font_size > 14) {
        font_size -= 2;
        text_w = (int)strlen(disp) * (font_size * 6 / 10);
    }
    int x = WINDOW_W - text_w - 12;
    int y = DISPLAY_H - font_size - 12;
    nv_draw_text_sized(c->surface, x, y, disp, COL_DISPLAY_FG, font_size);
}

static void draw_buttons(Calculator *c) {
    for (int row = 0; row < BTN_ROWS; row++) {
        for (int col = 0; col < BTN_COLS; col++) {
            const Button *b = &BUTTONS[row][col];
            int x = col * BTN_W;
            int y = DISPLAY_H + row * BTN_H;

            NvRect r = {x, y, BTN_W, BTN_H};
            nv_draw_fill_rect(c->surface, &r, btn_color(b));

            NvRect border = {x, y, BTN_W, 1};
            nv_draw_fill_rect(c->surface, &border, COL_BTN_BORDER);
            NvRect lborder = {x, y, 1, BTN_H};
            nv_draw_fill_rect(c->surface, &lborder, COL_BTN_BORDER);

            int label_w = (int)strlen(b->label) * 8;
            int lx = x + (BTN_W - label_w) / 2;
            int ly = y + (BTN_H - 14) / 2;
            nv_draw_text(c->surface, lx, ly, b->label, COL_BTN_FG);
        }
    }
}

static void draw_history(Calculator *c) {
    NvRect overlay = {0, 0, WINDOW_W, WINDOW_H};
    nv_draw_fill_rect(c->surface, &overlay, COL_HISTORY_BG);

    nv_draw_text(c->surface, 12, 12, "History", COL_DISPLAY_FG);

    NvRect sep = {0, 32, WINDOW_W, 1};
    nv_draw_fill_rect(c->surface, &sep, COL_HISTORY_SEP);

    int y = 44;
    int line_h = 36;
    int start = c->history_count - 1;
    int shown = 0;

    for (int i = start; i >= 0 && shown < 10; i--, shown++) {
        HistoryEntry *e = &c->history[i];
        nv_draw_text(c->surface, 12, y, e->expression, COL_HISTORY_FG);

        NvRect s = {0, y + line_h - 1, WINDOW_W, 1};
        nv_draw_fill_rect(c->surface, &s, COL_HISTORY_SEP);
        y += line_h;
    }

    if (c->history_count == 0) {
        nv_draw_text(c->surface, 12, 60, "No history yet", COL_EXPR_FG);
    }

    NvRect close_btn = {WINDOW_W - 60, 6, 52, 24};
    nv_draw_fill_rect(c->surface, &close_btn, COL_BTN_CLEAR);
    nv_draw_text(c->surface, WINDOW_W - 52, 11, "Close", COL_BTN_FG);
}

static void on_paint(NvWindow *win, NvSurface *surface, void *userdata) {
    (void)win;
    Calculator *c = (Calculator *)userdata;
    c->surface = surface;

    NvRect bg = {0, 0, WINDOW_W, WINDOW_H};
    nv_draw_fill_rect(surface, &bg, COL_DISPLAY_BG);

    if (c->show_history) {
        draw_history(c);
        return;
    }

    draw_display(c);
    draw_buttons(c);
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

static void on_mouse_down(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    Calculator *c = (Calculator *)userdata;

    if (c->show_history) {
        if (ev->x >= WINDOW_W - 60 && ev->x <= WINDOW_W - 8 &&
            ev->y >= 6 && ev->y <= 30) {
            c->show_history = 0;
            nv_surface_invalidate(c->surface);
        }
        return;
    }

    if (ev->y < DISPLAY_H) return;

    int col = ev->x / BTN_W;
    int row = (ev->y - DISPLAY_H) / BTN_H;

    if (col < 0 || col >= BTN_COLS) return;
    if (row < 0 || row >= BTN_ROWS) return;

    const Button *b = &BUTTONS[row][col];

    switch (b->type) {
        case BTN_DIGIT:      on_btn_digit(c, b->value);   break;
        case BTN_DECIMAL:    on_btn_decimal(c);            break;
        case BTN_OP:         on_btn_op(c, (Operator)b->value); break;
        case BTN_EQUALS:     calc_evaluate(c);             break;
        case BTN_CLEAR:      on_btn_clear(c);              break;
        case BTN_CLEAR_ENTRY:strcpy(c->display, "0");
                             c->decimal_entered = 0;       break;
        case BTN_BACKSPACE:  on_btn_backspace(c);          break;
        case BTN_NEGATE:     on_btn_negate(c);             break;
        case BTN_PERCENT:    on_btn_percent(c);            break;
        case BTN_SQRT:       on_btn_sqrt(c);               break;
        case BTN_SQUARE:     on_btn_square(c);             break;
        case BTN_RECIPROCAL: on_btn_reciprocal(c);         break;
        case BTN_HISTORY:    c->show_history = 1;          break;
    }

    nv_surface_invalidate(c->surface);
}

static void on_key_down(NvWindow *win, NvKeyEvent *ev, void *userdata) {
    (void)win;
    Calculator *c = (Calculator *)userdata;

    if (c->show_history) {
        if (ev->key == NV_KEY_ESCAPE) {
            c->show_history = 0;
            nv_surface_invalidate(c->surface);
        }
        return;
    }

    char ch = ev->codepoint;

    if (ch >= '0' && ch <= '9') { on_btn_digit(c, ch - '0'); goto done; }

    switch (ch) {
        case '+': on_btn_op(c, OP_ADD);  goto done;
        case '-': on_btn_op(c, OP_SUB);  goto done;
        case '*': on_btn_op(c, OP_MUL);  goto done;
        case '/': on_btn_op(c, OP_DIV);  goto done;
        case '%': on_btn_percent(c);      goto done;
        case '.': on_btn_decimal(c);      goto done;
        case '=':
        case '\r': calc_evaluate(c);      goto done;
    }

    switch (ev->key) {
        case NV_KEY_BACKSPACE: on_btn_backspace(c); break;
        case NV_KEY_ESCAPE:    on_btn_clear(c);     break;
        case NV_KEY_DELETE:    on_btn_clear(c);     break;
        default: return;
    }

done:
    nv_surface_invalidate(c->surface);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(void) {
    memset(&g_calc, 0, sizeof(Calculator));
    calc_reset(&g_calc);

    NvWindowConfig cfg;
    cfg.title    = "Calculator";
    cfg.width    = WINDOW_W;
    cfg.height   = WINDOW_H;
    cfg.resizable = 0;

    g_calc.window = nv_window_create(&cfg);
    if (!g_calc.window) return 1;

    nv_window_on_paint(g_calc.window,     on_paint,     &g_calc);
    nv_window_on_mouse_down(g_calc.window, on_mouse_down, &g_calc);
    nv_window_on_key_down(g_calc.window,  on_key_down,  &g_calc);

    return nv_app_run();
}