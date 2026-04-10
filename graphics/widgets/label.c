#include "label.h"
#include "../draw/draw.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>

Label *label_new(int x, int y, int w, int h, const char *text) {
    Label *lbl = malloc(sizeof(Label));
    if (!lbl) return NULL;
    memset(lbl, 0, sizeof(Label));
    widget_init(&lbl->base, x, y, w, h);
    if (text) strncpy(lbl->text, text, sizeof(lbl->text) - 1);
    lbl->color      = WT_FG;
    lbl->bg_color   = 0x00000000;
    lbl->font_size  = 13;
    lbl->align      = LABEL_ALIGN_LEFT;
    lbl->base.draw  = label_draw;
    return lbl;
}

void label_free(Label *lbl) {
    if (!lbl) return;
    widget_free_base(&lbl->base);
    free(lbl);
}

void label_set_text(Label *lbl, const char *text) {
    if (!lbl || !text) return;
    strncpy(lbl->text, text, sizeof(lbl->text) - 1);
    widget_set_dirty(&lbl->base);
}

void label_set_color(Label *lbl, uint32_t color)     { if (lbl) { lbl->color     = color; widget_set_dirty(&lbl->base); } }
void label_set_bg(Label *lbl, uint32_t bg)            { if (lbl) { lbl->bg_color  = bg;    widget_set_dirty(&lbl->base); } }
void label_set_font_size(Label *lbl, int size)        { if (lbl) { lbl->font_size = size;  widget_set_dirty(&lbl->base); } }
void label_set_align(Label *lbl, LabelAlign align)   { if (lbl) { lbl->align     = align; widget_set_dirty(&lbl->base); } }
void label_set_bold(Label *lbl, int bold)             { if (lbl) { lbl->bold      = bold;  widget_set_dirty(&lbl->base); } }
void label_set_wrap(Label *lbl, int wrap)             { if (lbl) { lbl->wrap      = wrap;  widget_set_dirty(&lbl->base); } }

void label_draw(Widget *w, NvSurface *dst) {
    if (!w || !dst) return;
    Label *lbl = (Label *)w;
    uint32_t col = (w->flags & WIDGET_DISABLED) ? WT_FG_DISABLED : lbl->color;

    if (COLOR_A(lbl->bg_color) > 0) {
        NvRect r = { w->x, w->y, w->w, w->h };
        draw_fill_rect(dst, &r, lbl->bg_color);
    }

    if (!lbl->text[0]) { w->flags &= ~WIDGET_DIRTY; return; }

    int tw, th;
    font_measure_string(lbl->text, lbl->font_size, &tw, &th);

    int tx = w->x + 2;
    int ty = w->y + (w->h - th) / 2;

    if (lbl->align == LABEL_ALIGN_CENTER)
        tx = w->x + (w->w - tw) / 2;
    else if (lbl->align == LABEL_ALIGN_RIGHT)
        tx = w->x + w->w - tw - 2;

    if (lbl->wrap && tw > w->w) {
        /* naive word-wrap: break at spaces when line would overflow */
        char buf[512];
        strncpy(buf, lbl->text, sizeof(buf) - 1);
        char *p = buf;
        int ly = ty;
        int cw_px = w->w > 4 ? w->w - 4 : w->w;
        while (*p) {
            int avail = cw_px;
            char *line_start = p;
            char *last_space = NULL;
            int used = 0;
            while (*p) {
                int gw = 7;
                if (used + gw > avail && last_space) {
                    p = last_space + 1;
                    break;
                }
                if (*p == ' ') last_space = p;
                if (*p == '\n') { p++; break; }
                used += gw; p++;
            }
            char save = *p; *p = '\0';
            font_draw_string_styled(dst, tx, ly, line_start,
                                    col, 0x00000000,
                                    lbl->font_size, lbl->bold, 0, lbl->underline);
            *p = save;
            ly += th + 2;
            if (ly > w->y + w->h) break;
        }
    } else {
        font_draw_string_styled(dst, tx, ty, lbl->text,
                                col, 0x00000000,
                                lbl->font_size, lbl->bold, 0, lbl->underline);
    }

    w->flags &= ~WIDGET_DIRTY;
}