#ifndef RENDERER_H
#define RENDERER_H

#include "html_parser.h"
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../libnv0/include/nv0/window.h"

typedef enum {
    BOX_RECT,
    BOX_HR,
    BOX_IMAGE,
} RenderBoxType;

typedef struct RenderBox {
    RenderBoxType    type;
    int              x, y;
    int              width, height;
    NvColor          color;
    NvColor          bg_color;
    int              font_size;
    char            *text;
    char            *href;
    struct RenderBox *parent;
    struct RenderBox **children;
    int               child_count;
    int               child_cap;
} RenderBox;

typedef struct {
    char   *text;
    char   *href;
    int     x;
    int     width;
    int     height;
    int     font_size;
    int     bold;
    int     italic;
    int     underline;
    int     strikethrough;
    int     is_code;
    NvColor color;
    NvColor bg_color;
} TextSpan;

typedef struct {
    TextSpan **spans;
    int        span_count;
    int        span_cap;
    int        y;
    int        height;
    int        baseline;
} LineBox;

typedef struct {
    LineBox   **lines;
    int         line_count;
    int         line_cap;
    RenderBox **boxes;
    int         box_count;
    int         box_cap;
    int         page_height;
    int         page_width;
} RenderOutput;

typedef struct {
    int viewport_w;
    int viewport_h;
    int base_font_size;
} RenderConfig;

RenderOutput *renderer_render(HtmlDocument *doc, RenderConfig *cfg);
void          renderer_output_free(RenderOutput *out);
void          renderer_draw(NvSurface *surface, RenderOutput *out, int off_x, int off_y);
const char   *renderer_hit_test(RenderOutput *out, int x, int y);
int           renderer_page_height(RenderOutput *out);

#endif