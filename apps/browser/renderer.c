#include "renderer.h"
#include "html_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define DEFAULT_FONT_SIZE    14
#define LINE_HEIGHT_FACTOR   1.5f
#define PARAGRAPH_MARGIN     12
#define HEADING_MARGIN_TOP   16
#define HEADING_MARGIN_BOT   8
#define BLOCK_PADDING        4
#define LIST_INDENT          24
#define LIST_BULLET_X        8
#define CODE_PADDING         8
#define BLOCKQUOTE_INDENT    20
#define BLOCKQUOTE_BAR_W     4
#define HR_HEIGHT            1
#define HR_MARGIN            12
#define LINK_COLOR           NV_COLOR(0x00, 0x66, 0xCC, 0xFF)
#define TEXT_COLOR           NV_COLOR(0x11, 0x11, 0x11, 0xFF)
#define CODE_BG_COLOR        NV_COLOR(0xF4, 0xF4, 0xF4, 0xFF)
#define BLOCKQUOTE_COLOR     NV_COLOR(0x77, 0x77, 0x77, 0xFF)
#define BLOCKQUOTE_BAR_COLOR NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define H1_SIZE              28
#define H2_SIZE              22
#define H3_SIZE              18
#define H4_SIZE              16
#define H5_SIZE              14
#define H6_SIZE              13
#define MAX_BOX_CHILDREN     256
#define INITIAL_BOX_CAP      32
#define INITIAL_SPAN_CAP     16

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
 * Font metrics — thin wrapper so renderer is not coupled to a specific
 * font engine. nv_font_measure() and nv_font_draw() are provided by the
 * graphics layer. These stubs make the logic self-contained.
 * --------------------------------------------------------------------- */

static int font_char_width(int font_size) {
    return (font_size * 6) / 10;
}

static int font_measure_text(const char *text, int font_size) {
    if (!text) return 0;
    return (int)strlen(text) * font_char_width(font_size);
}

static int font_line_height(int font_size) {
    return (int)(font_size * LINE_HEIGHT_FACTOR);
}

/* -----------------------------------------------------------------------
 * RenderBox — the core layout unit
 * --------------------------------------------------------------------- */

static RenderBox *box_new(RenderBoxType type) {
    RenderBox *b = xmalloc(sizeof(RenderBox));
    if (!b) return NULL;
    memset(b, 0, sizeof(RenderBox));
    b->type      = type;
    b->color     = TEXT_COLOR;
    b->bg_color  = NV_COLOR(0, 0, 0, 0);
    b->font_size = DEFAULT_FONT_SIZE;
    return b;
}

static void box_free(RenderBox *b) {
    if (!b) return;
    free(b->text);
    free(b->href);
    for (int i = 0; i < b->child_count; i++) box_free(b->children[i]);
    free(b->children);
    free(b);
}

static int box_add_child(RenderBox *parent, RenderBox *child) {
    if (parent->child_count >= parent->child_cap) {
        int nc = parent->child_cap == 0 ? INITIAL_BOX_CAP : parent->child_cap * 2;
        RenderBox **nb = xrealloc(parent->children, sizeof(RenderBox *) * nc);
        if (!nb) return -1;
        parent->children  = nb;
        parent->child_cap = nc;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    return 0;
}

/* -----------------------------------------------------------------------
 * TextSpan — a run of text with uniform style within a line
 * --------------------------------------------------------------------- */

static TextSpan *span_new(void) {
    TextSpan *s = xmalloc(sizeof(TextSpan));
    if (!s) return NULL;
    memset(s, 0, sizeof(TextSpan));
    s->color     = TEXT_COLOR;
    s->font_size = DEFAULT_FONT_SIZE;
    return s;
}

static void span_free(TextSpan *s) {
    if (!s) return;
    free(s->text);
    free(s->href);
    free(s);
}

/* -----------------------------------------------------------------------
 * LineBox — a horizontal run of spans that fits within viewport_w
 * --------------------------------------------------------------------- */

static LineBox *linebox_new(void) {
    LineBox *lb = xmalloc(sizeof(LineBox));
    if (!lb) return NULL;
    lb->spans     = NULL;
    lb->span_count = 0;
    lb->span_cap  = 0;
    lb->y         = 0;
    lb->height    = 0;
    lb->baseline  = 0;
    return lb;
}

static void linebox_free(LineBox *lb) {
    if (!lb) return;
    for (int i = 0; i < lb->span_count; i++) span_free(lb->spans[i]);
    free(lb->spans);
    free(lb);
}

static int linebox_add_span(LineBox *lb, TextSpan *s) {
    if (lb->span_count >= lb->span_cap) {
        int nc = lb->span_cap == 0 ? INITIAL_SPAN_CAP : lb->span_cap * 2;
        TextSpan **nb = xrealloc(lb->spans, sizeof(TextSpan *) * nc);
        if (!nb) return -1;
        lb->spans    = nb;
        lb->span_cap = nc;
    }
    lb->spans[lb->span_count++] = s;
    if (s->height > lb->height) lb->height = s->height;
    return 0;
}

/* -----------------------------------------------------------------------
 * RenderOutput
 * --------------------------------------------------------------------- */

RenderOutput *renderer_output_new(void) {
    RenderOutput *out = xmalloc(sizeof(RenderOutput));
    if (!out) return NULL;
    out->lines      = NULL;
    out->line_count = 0;
    out->line_cap   = 0;
    out->boxes      = NULL;
    out->box_count  = 0;
    out->box_cap    = 0;
    out->page_height = 0;
    out->page_width  = 0;
    return out;
}

void renderer_output_free(RenderOutput *out) {
    if (!out) return;
    for (int i = 0; i < out->line_count; i++) linebox_free(out->lines[i]);
    free(out->lines);
    for (int i = 0; i < out->box_count; i++) box_free(out->boxes[i]);
    free(out->boxes);
    free(out);
}

static int output_add_line(RenderOutput *out, LineBox *lb) {
    if (out->line_count >= out->line_cap) {
        int nc = out->line_cap == 0 ? 64 : out->line_cap * 2;
        LineBox **nb = xrealloc(out->lines, sizeof(LineBox *) * nc);
        if (!nb) return -1;
        out->lines    = nb;
        out->line_cap = nc;
    }
    out->lines[out->line_count++] = lb;
    return 0;
}

static int output_add_box(RenderOutput *out, RenderBox *b) {
    if (out->box_count >= out->box_cap) {
        int nc = out->box_cap == 0 ? 32 : out->box_cap * 2;
        RenderBox **nb = xrealloc(out->boxes, sizeof(RenderBox *) * nc);
        if (!nb) return -1;
        out->boxes    = nb;
        out->box_cap  = nc;
    }
    out->boxes[out->box_count++] = b;
    return 0;
}

/* -----------------------------------------------------------------------
 * Style context — tracks inherited CSS-like properties during tree walk
 * --------------------------------------------------------------------- */

typedef struct {
    int   font_size;
    int   bold;
    int   italic;
    int   underline;
    int   strikethrough;
    int   is_link;
    int   is_code;
    int   is_pre;
    int   list_depth;
    int   list_ordered;
    int   list_index;
    int   indent;
    NvColor color;
    NvColor bg_color;
    char *href;
} StyleCtx;

static void style_init(StyleCtx *s, RenderConfig *cfg) {
    memset(s, 0, sizeof(StyleCtx));
    s->font_size = cfg->base_font_size;
    s->color     = TEXT_COLOR;
    s->bg_color  = NV_COLOR(0, 0, 0, 0);
}

static StyleCtx style_push(StyleCtx *s) {
    return *s;
}

/* -----------------------------------------------------------------------
 * Word-wrap inline text into LineBoxes
 * --------------------------------------------------------------------- */

typedef struct {
    RenderOutput *out;
    RenderConfig *cfg;
    int           cursor_x;
    int           cursor_y;
    int           indent;
    LineBox      *current_line;
} LayoutCtx;

static void layout_flush_line(LayoutCtx *lc) {
    if (!lc->current_line) return;
    if (lc->current_line->span_count == 0) {
        linebox_free(lc->current_line);
        lc->current_line = NULL;
        return;
    }
    lc->current_line->y = lc->cursor_y;
    if (lc->current_line->height == 0)
        lc->current_line->height = font_line_height(DEFAULT_FONT_SIZE);
    output_add_line(lc->out, lc->current_line);
    lc->cursor_y     += lc->current_line->height;
    lc->cursor_x      = lc->indent;
    lc->current_line  = NULL;
}

static LineBox *layout_ensure_line(LayoutCtx *lc) {
    if (!lc->current_line) {
        lc->current_line = linebox_new();
        if (!lc->current_line) return NULL;
        lc->cursor_x = lc->indent;
    }
    return lc->current_line;
}

static void layout_add_span(LayoutCtx *lc, StyleCtx *style,
                             const char *word, const char *href) {
    int word_w  = font_measure_text(word, style->font_size);
    int line_h  = font_line_height(style->font_size);
    int max_w   = lc->cfg->viewport_w - lc->indent;

    if (lc->current_line && lc->cursor_x + word_w > lc->cfg->viewport_w) {
        layout_flush_line(lc);
    }

    LineBox *lb = layout_ensure_line(lc);
    if (!lb) return;

    TextSpan *sp = span_new();
    if (!sp) return;

    sp->text        = xstrdup(word);
    sp->x           = lc->cursor_x;
    sp->width       = word_w;
    sp->height      = line_h;
    sp->font_size   = style->font_size;
    sp->bold        = style->bold;
    sp->italic      = style->italic;
    sp->underline   = style->underline || style->is_link;
    sp->strikethrough = style->strikethrough;
    sp->color       = style->is_link ? LINK_COLOR : style->color;
    sp->is_code     = style->is_code;
    sp->href        = href ? xstrdup(href) : NULL;

    if (word_w > max_w) word_w = max_w;
    lc->cursor_x += word_w + font_char_width(style->font_size);

    linebox_add_span(lb, sp);
}

static void layout_text(LayoutCtx *lc, StyleCtx *style, const char *text) {
    if (!text || !text[0]) return;

    if (style->is_pre) {
        layout_flush_line(lc);
        const char *p = text;
        while (*p) {
            const char *nl = strchr(p, '\n');
            size_t seg_len = nl ? (size_t)(nl - p) : strlen(p);
            char *seg = xmalloc(seg_len + 1);
            if (seg) {
                memcpy(seg, p, seg_len);
                seg[seg_len] = '\0';
                layout_add_span(lc, style, seg, NULL);
                free(seg);
            }
            layout_flush_line(lc);
            if (!nl) break;
            p = nl + 1;
        }
        return;
    }

    char *buf = xstrdup(text);
    if (!buf) return;

    char *p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;

        char *word_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        char saved = *p;
        *p = '\0';

        layout_add_span(lc, style, word_start, style->is_link ? style->href : NULL);

        *p = saved;
    }

    free(buf);
}

static void layout_margin(LayoutCtx *lc, int amount) {
    layout_flush_line(lc);
    lc->cursor_y += amount;
}

/* -----------------------------------------------------------------------
 * Block-level box emitters
 * --------------------------------------------------------------------- */

static void emit_hr(LayoutCtx *lc) {
    layout_flush_line(lc);
    lc->cursor_y += HR_MARGIN;

    RenderBox *b = box_new(BOX_HR);
    b->x      = lc->indent;
    b->y      = lc->cursor_y;
    b->width  = lc->cfg->viewport_w - lc->indent * 2;
    b->height = HR_HEIGHT;
    b->color  = NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF);
    output_add_box(lc->out, b);

    lc->cursor_y += HR_HEIGHT + HR_MARGIN;
}

static void emit_code_block(LayoutCtx *lc, StyleCtx *style, const char *text) {
    layout_flush_line(lc);
    lc->cursor_y += BLOCK_PADDING;

    int lines = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') lines++;
    int block_h = lines * font_line_height(style->font_size) + CODE_PADDING * 2;

    RenderBox *bg = box_new(BOX_RECT);
    bg->x        = lc->indent;
    bg->y        = lc->cursor_y;
    bg->width    = lc->cfg->viewport_w - lc->indent * 2;
    bg->height   = block_h;
    bg->bg_color = CODE_BG_COLOR;
    bg->color    = NV_COLOR(0, 0, 0, 0);
    output_add_box(lc->out, bg);

    int saved_indent = lc->indent;
    lc->indent    = lc->indent + CODE_PADDING;
    lc->cursor_y += CODE_PADDING;

    layout_text(lc, style, text);
    layout_flush_line(lc);

    lc->cursor_y  += CODE_PADDING + BLOCK_PADDING;
    lc->indent     = saved_indent;
}

static void emit_blockquote_bar(LayoutCtx *lc, int y_start, int y_end) {
    RenderBox *bar = box_new(BOX_RECT);
    bar->x        = lc->indent - BLOCKQUOTE_INDENT + 4;
    bar->y        = y_start;
    bar->width    = BLOCKQUOTE_BAR_W;
    bar->height   = y_end - y_start;
    bar->bg_color = BLOCKQUOTE_BAR_COLOR;
    bar->color    = NV_COLOR(0, 0, 0, 0);
    output_add_box(lc->out, bar);
}

static void emit_list_bullet(LayoutCtx *lc, StyleCtx *style,
                              int ordered, int index) {
    layout_ensure_line(lc);
    char bullet[16];
    if (ordered)
        snprintf(bullet, sizeof(bullet), "%d.", index);
    else
        snprintf(bullet, sizeof(bullet), "\xE2\x80\xA2");

    StyleCtx bullet_style = *style;
    bullet_style.is_link  = 0;
    bullet_style.href     = NULL;

    int saved_x   = lc->cursor_x;
    lc->cursor_x  = lc->indent - LIST_INDENT + LIST_BULLET_X;
    layout_add_span(lc, &bullet_style, bullet, NULL);
    lc->cursor_x  = saved_x;
}

/* -----------------------------------------------------------------------
 * Recursive tree walk — converts HTML nodes into layout
 * --------------------------------------------------------------------- */

static void walk_node(LayoutCtx *lc, StyleCtx *style, HtmlNode *node);

static void walk_children(LayoutCtx *lc, StyleCtx *style, HtmlNode *node) {
    for (int i = 0; i < node->child_count; i++)
        walk_node(lc, style, node->children[i]);
}

static int heading_font_size(const char *tag) {
    if (!tag) return DEFAULT_FONT_SIZE;
    if (tag[0] == 'h' || tag[0] == 'H') {
        switch (tag[1]) {
            case '1': return H1_SIZE;
            case '2': return H2_SIZE;
            case '3': return H3_SIZE;
            case '4': return H4_SIZE;
            case '5': return H5_SIZE;
            case '6': return H6_SIZE;
        }
    }
    return DEFAULT_FONT_SIZE;
}

static int is_heading(const char *tag) {
    if (!tag) return 0;
    return (tag[0] == 'h' || tag[0] == 'H') &&
           tag[1] >= '1' && tag[1] <= '6' && tag[2] == '\0';
}

static int is_block(const char *tag) {
    if (!tag) return 0;
    static const char *blocks[] = {
        "div","p","section","article","aside","nav","header","footer",
        "main","figure","figcaption","address","fieldset","details",
        "summary","dialog","form","table","thead","tbody","tfoot","tr",
        "caption","ul","ol","li","dl","dt","dd","blockquote","pre",
        "h1","h2","h3","h4","h5","h6","hr","br","noscript",NULL
    };
    for (int i = 0; blocks[i]; i++)
        if (strcasecmp(tag, blocks[i]) == 0) return 1;
    return 0;
}

static void walk_node(LayoutCtx *lc, StyleCtx *style, HtmlNode *node) {
    if (!node) return;

    if (node->type == HTML_NODE_TEXT) {
        if (node->text && node->text[0])
            layout_text(lc, style, node->text);
        return;
    }

    if (node->type == HTML_NODE_COMMENT) return;

    if (node->type == HTML_NODE_DOCUMENT) {
        walk_children(lc, style, node);
        return;
    }

    if (node->type != HTML_NODE_ELEMENT || !node->tag) return;

    const char *tag = node->tag;

    if (strcasecmp(tag, "head")   == 0) return;
    if (strcasecmp(tag, "script") == 0) return;
    if (strcasecmp(tag, "style")  == 0) return;
    if (strcasecmp(tag, "meta")   == 0) return;
    if (strcasecmp(tag, "link")   == 0) return;
    if (strcasecmp(tag, "title")  == 0) return;

    StyleCtx child_style = style_push(style);

    if (strcasecmp(tag, "b") == 0 || strcasecmp(tag, "strong") == 0)
        child_style.bold = 1;

    if (strcasecmp(tag, "i") == 0 || strcasecmp(tag, "em") == 0)
        child_style.italic = 1;

    if (strcasecmp(tag, "u") == 0 || strcasecmp(tag, "ins") == 0)
        child_style.underline = 1;

    if (strcasecmp(tag, "s") == 0 || strcasecmp(tag, "del") == 0 ||
        strcasecmp(tag, "strike") == 0)
        child_style.strikethrough = 1;

    if (strcasecmp(tag, "code") == 0 || strcasecmp(tag, "tt") == 0 ||
        strcasecmp(tag, "kbd")  == 0 || strcasecmp(tag, "samp") == 0) {
        child_style.is_code = 1;
        child_style.font_size = (int)(style->font_size * 0.9f);
    }

    if (strcasecmp(tag, "small") == 0)
        child_style.font_size = (int)(style->font_size * 0.85f);

    if (strcasecmp(tag, "big") == 0 || strcasecmp(tag, "sup") == 0 ||
        strcasecmp(tag, "sub") == 0)
        child_style.font_size = (int)(style->font_size * 1.1f);

    if (strcasecmp(tag, "mark") == 0)
        child_style.bg_color = NV_COLOR(0xFF, 0xFF, 0x00, 0xFF);

    if (is_heading(tag)) {
        layout_flush_line(lc);
        lc->cursor_y += HEADING_MARGIN_TOP;
        child_style.bold      = 1;
        child_style.font_size = heading_font_size(tag);
        walk_children(lc, &child_style, node);
        layout_flush_line(lc);
        lc->cursor_y += HEADING_MARGIN_BOT;
        return;
    }

    if (strcasecmp(tag, "p") == 0) {
        layout_flush_line(lc);
        lc->cursor_y += PARAGRAPH_MARGIN;
        walk_children(lc, &child_style, node);
        layout_flush_line(lc);
        lc->cursor_y += PARAGRAPH_MARGIN;
        return;
    }

    if (strcasecmp(tag, "br") == 0) {
        layout_flush_line(lc);
        lc->cursor_y += font_line_height(style->font_size);
        return;
    }

    if (strcasecmp(tag, "hr") == 0) {
        emit_hr(lc);
        return;
    }

    if (strcasecmp(tag, "pre") == 0) {
        layout_flush_line(lc);
        lc->cursor_y += BLOCK_PADDING;
        child_style.is_pre  = 1;
        child_style.is_code = 1;
        char *text = html_text_content(node);
        if (text) {
            emit_code_block(lc, &child_style, text);
            free(text);
        }
        return;
    }

    if (strcasecmp(tag, "blockquote") == 0) {
        layout_flush_line(lc);
        int y_start   = lc->cursor_y;
        int saved_ind = lc->indent;
        lc->cursor_y += BLOCK_PADDING;
        lc->indent   += BLOCKQUOTE_INDENT;
        child_style.color = BLOCKQUOTE_COLOR;
        walk_children(lc, &child_style, node);
        layout_flush_line(lc);
        lc->indent    = saved_ind;
        lc->cursor_y += BLOCK_PADDING;
        emit_blockquote_bar(lc, y_start, lc->cursor_y);
        return;
    }

    if (strcasecmp(tag, "ul") == 0 || strcasecmp(tag, "ol") == 0) {
        layout_flush_line(lc);
        int saved_ind    = lc->indent;
        int saved_depth  = child_style.list_depth;
        int saved_ord    = child_style.list_ordered;
        int saved_idx    = child_style.list_index;

        child_style.list_depth++;
        child_style.list_ordered = (strcasecmp(tag, "ol") == 0);
        child_style.list_index   = 1;
        lc->indent += LIST_INDENT;

        walk_children(lc, &child_style, node);
        layout_flush_line(lc);

        lc->indent = saved_ind;
        child_style.list_depth   = saved_depth;
        child_style.list_ordered = saved_ord;
        child_style.list_index   = saved_idx;
        return;
    }

    if (strcasecmp(tag, "li") == 0) {
        layout_flush_line(lc);
        emit_list_bullet(lc, &child_style,
                         child_style.list_ordered,
                         child_style.list_index);
        child_style.list_index++;
        walk_children(lc, &child_style, node);
        layout_flush_line(lc);
        return;
    }

    if (strcasecmp(tag, "a") == 0) {
        const char *href = html_attr(node, "href");
        child_style.is_link = 1;
        child_style.href    = (char *)href;
        child_style.color   = LINK_COLOR;
        walk_children(lc, &child_style, node);
        return;
    }

    if (strcasecmp(tag, "img") == 0) {
        const char *src = html_attr(node, "src");
        const char *alt = html_attr(node, "alt");
        int w = 0, h = 0;
        const char *ws = html_attr(node, "width");
        const char *hs = html_attr(node, "height");
        if (ws) w = atoi(ws);
        if (hs) h = atoi(hs);
        if (w <= 0) w = 200;
        if (h <= 0) h = 150;

        if (lc->cursor_x + w > lc->cfg->viewport_w)
            layout_flush_line(lc);

        RenderBox *img_box = box_new(BOX_IMAGE);
        img_box->x      = lc->cursor_x;
        img_box->y      = lc->cursor_y;
        img_box->width  = w;
        img_box->height = h;
        img_box->text   = src ? xstrdup(src) : NULL;
        img_box->href   = alt ? xstrdup(alt) : NULL;
        output_add_box(lc->out, img_box);

        lc->cursor_x += w + BLOCK_PADDING;
        if (lc->cursor_x >= lc->cfg->viewport_w) {
            layout_flush_line(lc);
            lc->cursor_y += h;
        }
        return;
    }

    if (strcasecmp(tag, "table") == 0) {
        layout_flush_line(lc);
        lc->cursor_y += BLOCK_PADDING;

        int col_w = (lc->cfg->viewport_w - lc->indent) / 4;

        for (int r = 0; r < node->child_count; r++) {
            HtmlNode *section = node->children[r];
            if (!section) continue;
            HtmlNode *rows = section;
            if (strcasecmp(section->tag ? section->tag : "", "thead") == 0 ||
                strcasecmp(section->tag ? section->tag : "", "tbody") == 0 ||
                strcasecmp(section->tag ? section->tag : "", "tfoot") == 0) {
                rows = section;
            }
            for (int ri = 0; ri < rows->child_count; ri++) {
                HtmlNode *row = rows->children[ri];
                if (!row || !row->tag) continue;
                if (strcasecmp(row->tag, "tr") != 0) continue;

                int col_x = lc->indent;
                int row_y = lc->cursor_y;
                int max_h = font_line_height(style->font_size);

                for (int ci = 0; ci < row->child_count; ci++) {
                    HtmlNode *cell = row->children[ci];
                    if (!cell || !cell->tag) continue;
                    if (strcasecmp(cell->tag, "td") != 0 &&
                        strcasecmp(cell->tag, "th") != 0) continue;

                    StyleCtx cell_style = *style;
                    if (strcasecmp(cell->tag, "th") == 0) cell_style.bold = 1;

                    LayoutCtx cell_lc;
                    cell_lc.out          = lc->out;
                    cell_lc.cfg          = lc->cfg;
                    cell_lc.cursor_x     = col_x + BLOCK_PADDING;
                    cell_lc.cursor_y     = row_y;
                    cell_lc.indent       = col_x + BLOCK_PADDING;
                    cell_lc.current_line = NULL;

                    walk_children(&cell_lc, &cell_style, cell);
                    layout_flush_line(&cell_lc);

                    int cell_h = cell_lc.cursor_y - row_y;
                    if (cell_h > max_h) max_h = cell_h;

                    col_x += col_w;
                    if (ci >= 3) break;
                }
                lc->cursor_y = row_y + max_h + BLOCK_PADDING;
            }
        }
        lc->cursor_y += BLOCK_PADDING;
        return;
    }

    if (is_block(tag)) {
        layout_flush_line(lc);
    }

    walk_children(lc, &child_style, node);

    if (is_block(tag)) {
        layout_flush_line(lc);
    }
}

/* -----------------------------------------------------------------------
 * Public: renderer_render
 * --------------------------------------------------------------------- */

RenderOutput *renderer_render(HtmlDocument *doc, RenderConfig *cfg) {
    if (!doc || !cfg) return NULL;

    RenderOutput *out = renderer_output_new();
    if (!out) return NULL;

    StyleCtx style;
    style_init(&style, cfg);

    LayoutCtx lc;
    lc.out          = out;
    lc.cfg          = cfg;
    lc.cursor_x     = 0;
    lc.cursor_y     = cfg->base_font_size;
    lc.indent       = cfg->base_font_size;
    lc.current_line = NULL;

    walk_node(&lc, &style, doc->root);
    layout_flush_line(&lc);

    out->page_height = lc.cursor_y + cfg->base_font_size;
    out->page_width  = cfg->viewport_w;

    return out;
}

/* -----------------------------------------------------------------------
 * Public: renderer_draw
 * --------------------------------------------------------------------- */

void renderer_draw(NvSurface *surface, RenderOutput *out, int off_x, int off_y) {
    if (!surface || !out) return;

    for (int i = 0; i < out->box_count; i++) {
        RenderBox *b = out->boxes[i];
        NvRect r = { b->x + off_x, b->y + off_y, b->width, b->height };

        switch (b->type) {
            case BOX_RECT:
                nv_draw_fill_rect(surface, &r, b->bg_color);
                break;
            case BOX_HR:
                nv_draw_fill_rect(surface, &r, b->color);
                break;
            case BOX_IMAGE:
                nv_draw_rect(surface, &r, NV_COLOR(0xAA, 0xAA, 0xAA, 0xFF));
                if (b->href)
                    nv_draw_text(surface, r.x + 4, r.y + r.h / 2,
                                 b->href, NV_COLOR(0x77, 0x77, 0x77, 0xFF));
                break;
            default:
                break;
        }
    }

    for (int i = 0; i < out->line_count; i++) {
        LineBox *lb = out->lines[i];
        int line_y  = lb->y + off_y;

        for (int j = 0; j < lb->span_count; j++) {
            TextSpan *sp = lb->spans[j];
            int x = sp->x + off_x;
            int y = line_y;

            if (sp->is_code) {
                NvRect code_bg = { x - 2, y, sp->width + 4, sp->height };
                nv_draw_fill_rect(surface, &code_bg, CODE_BG_COLOR);
            }

            nv_draw_text_styled(surface, x, y, sp->text,
                                sp->color, sp->font_size,
                                sp->bold, sp->italic);

            if (sp->underline) {
                NvRect ul = { x, y + sp->height - 2, sp->width, 1 };
                nv_draw_fill_rect(surface, &ul, sp->color);
            }

            if (sp->strikethrough) {
                NvRect st = { x, y + sp->height / 2, sp->width, 1 };
                nv_draw_fill_rect(surface, &st, sp->color);
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Public: renderer_hit_test
 * --------------------------------------------------------------------- */

const char *renderer_hit_test(RenderOutput *out, int x, int y) {
    if (!out) return NULL;
    for (int i = 0; i < out->line_count; i++) {
        LineBox *lb = out->lines[i];
        if (y < lb->y || y >= lb->y + lb->height) continue;
        for (int j = 0; j < lb->span_count; j++) {
            TextSpan *sp = lb->spans[j];
            if (x >= sp->x && x < sp->x + sp->width && sp->href)
                return sp->href;
        }
    }
    return NULL;
}

/* -----------------------------------------------------------------------
 * Public: renderer_page_height
 * --------------------------------------------------------------------- */

int renderer_page_height(RenderOutput *out) {
    if (!out) return 0;
    return out->page_height;
}