#include "font_render.h"
#include "psf.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Global default renderer
 * --------------------------------------------------------------------- */

static FontRenderer *g_default  = NULL;
static PSFFont      *g_psf      = NULL;

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

/* size → scale factor for the 8×16 builtin glyph */
static int size_to_scale(int size) {
    if (size <= 10) return 1;
    if (size <= 16) return 1;
    if (size <= 24) return 2;
    if (size <= 32) return 2;
    return 3;
}

/* size → approximate pixel width per column */
static int size_to_cw(int size) {
    if (size <= 10) return 6;
    if (size <= 13) return 7;
    if (size <= 16) return 8;
    if (size <= 20) return 10;
    if (size <= 24) return 12;
    return 14;
}

/* -----------------------------------------------------------------------
 * FontRenderer
 * --------------------------------------------------------------------- */

FontRenderer *font_renderer_new(PSFFont *font, int scale) {
    FontRenderer *fr = xmalloc(sizeof(FontRenderer));
    if (!fr) return NULL;
    fr->font        = font ? font : font_get_default_psf();
    fr->scale       = scale > 0 ? scale : 1;
    fr->tab_width   = 8;
    if (fr->font) {
        fr->glyph_w    = fr->font->width  * fr->scale;
        fr->glyph_h    = fr->font->height * fr->scale;
        fr->line_height = fr->glyph_h + 2;
    }
    return fr;
}

void font_renderer_free(FontRenderer *fr) { free(fr); }

void font_renderer_set_font(FontRenderer *fr, PSFFont *font) {
    if (!fr || !font) return;
    fr->font   = font;
    fr->glyph_w = font->width  * fr->scale;
    fr->glyph_h = font->height * fr->scale;
    fr->line_height = fr->glyph_h + 2;
}

void font_renderer_set_scale(FontRenderer *fr, int scale) {
    if (!fr || scale <= 0) return;
    fr->scale   = scale;
    if (fr->font) {
        fr->glyph_w    = fr->font->width  * scale;
        fr->glyph_h    = fr->font->height * scale;
        fr->line_height = fr->glyph_h + 2;
    }
}

int font_glyph_width(const FontRenderer *fr) {
    return fr ? fr->glyph_w : 8;
}
int font_glyph_height(const FontRenderer *fr) {
    return fr ? fr->glyph_h : 16;
}
int font_line_height(const FontRenderer *fr) {
    return fr ? fr->line_height : 18;
}

int font_string_width(const FontRenderer *fr, const char *str) {
    if (!str) return 0;
    return (int)strlen(str) * (fr ? fr->glyph_w : 8);
}

int font_string_width_n(const FontRenderer *fr, const char *str, int n) {
    if (!str) return 0;
    int cw = fr ? fr->glyph_w : 8;
    int count = 0;
    for (int i = 0; i < n && str[i]; i++) count++;
    return count * cw;
}

/* -----------------------------------------------------------------------
 * Core glyph rasterizer
 * Renders one PSF glyph at (x,y) at pixel scale `scale`.
 * Bold renders each pixel twice (one pixel right).
 * Italic shears by shifting left proportionally to row.
 * --------------------------------------------------------------------- */

static void rasterize_glyph(NvSurface *surface,
                              const PSFFont *font, int glyph_idx,
                              int x, int y, uint32_t fg,
                              int scale, int bold, int italic) {
    if (!surface || !font || !font->glyph_data) return;
    if (glyph_idx < 0 || glyph_idx >= font->num_glyphs) return;

    const uint8_t *glyph   = font->glyph_data + glyph_idx * font->bytes_per_glyph;
    int            bpr     = (font->width + 7) / 8;
    int            half_h  = font->height / 2;

    for (int gy = 0; gy < font->height; gy++) {
        int italic_shift = italic ? (half_h - gy) / 4 : 0;

        for (int gx = 0; gx < font->width; gx++) {
            uint8_t byte = glyph[gy * bpr + gx / 8];
            int bit = (byte >> (7 - (gx & 7))) & 1;
            if (!bit) continue;

            int px = x + gx * scale + italic_shift;
            int py = y + gy * scale;

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    draw_pixel(surface, px + sx, py + sy, fg);
                }
                if (bold) {
                    for (int sx = 0; sx < scale; sx++)
                        draw_pixel(surface, px + sx + 1, py + sy, fg);
                }
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Public draw functions
 * --------------------------------------------------------------------- */

void font_draw_char(NvSurface *surface, int x, int y,
                    char c, uint32_t color, int size, int bold) {
    PSFFont *font = font_get_default_psf();
    if (!font) return;
    int scale = size_to_scale(size);
    int glyph_idx;

    if ((unsigned char)c >= 0x20 && (unsigned char)c <= 0x7E)
        glyph_idx = psf_unicode_to_glyph(font, (uint32_t)(unsigned char)c);
    else
        glyph_idx = psf_unicode_to_glyph(font, '?');

    rasterize_glyph(surface, font, glyph_idx, x, y, color,
                    scale, bold, 0);
}

void font_draw_char_scaled(NvSurface *surface, int x, int y,
                            uint32_t codepoint, uint32_t color,
                            int scale, const PSFFont *font) {
    if (!font) font = font_get_default_psf();
    if (!font) return;
    int idx = psf_unicode_to_glyph(font, codepoint);
    rasterize_glyph(surface, font, idx, x, y, color, scale, 0, 0);
}

void font_draw_string(NvSurface *surface, int x, int y,
                      const char *str, uint32_t color, int size) {
    if (!surface || !str) return;
    PSFFont *font = font_get_default_psf();
    if (!font) return;

    int scale = size_to_scale(size);
    int cw    = font->width  * scale;
    int ch    = font->height * scale;
    int cx    = x;

    for (int i = 0; str[i]; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '\n') {
            cx  = x;
            y  += ch + 2;
            continue;
        }
        if (c == '\t') {
            int tab = 8 * cw;
            cx = ((cx - x) / tab + 1) * tab + x;
            continue;
        }
        int idx = psf_unicode_to_glyph(font, c);
        rasterize_glyph(surface, font, idx, cx, y, color, scale, 0, 0);
        cx += cw;
    }
}

void font_draw_string_clip(NvSurface *surface, int x, int y,
                            const char *str, uint32_t color,
                            int size, int max_w) {
    if (!surface || !str || max_w <= 0) return;
    PSFFont *font = font_get_default_psf();
    if (!font) return;

    int scale = size_to_scale(size);
    int cw    = font->width * scale;
    int cx    = x;
    int limit = x + max_w;
    int len   = (int)strlen(str);

    int ellipsis_w = cw * 3;
    int text_limit = limit;
    int needs_ellipsis = 0;

    if (x + len * cw > limit) {
        needs_ellipsis = 1;
        text_limit = limit - ellipsis_w;
    }

    for (int i = 0; str[i] && cx + cw <= text_limit; i++) {
        unsigned char c = (unsigned char)str[i];
        int idx = psf_unicode_to_glyph(font, c);
        rasterize_glyph(surface, font, idx, cx, y, color, scale, 0, 0);
        cx += cw;
    }

    if (needs_ellipsis) {
        for (int k = 0; k < 3 && cx + cw <= limit; k++) {
            int idx = psf_unicode_to_glyph(font, '.');
            rasterize_glyph(surface, font, idx, cx, y, color, scale, 0, 0);
            cx += cw;
        }
    }
}

void font_draw_string_styled(NvSurface *surface, int x, int y,
                              const char *str, uint32_t fg,
                              uint32_t bg, int size,
                              int bold, int italic, int underline) {
    if (!surface || !str) return;
    PSFFont *font = font_get_default_psf();
    if (!font) return;

    int scale = size_to_scale(size);
    int cw    = font->width  * scale;
    int ch    = font->height * scale;
    int len   = (int)strlen(str);

    if (COLOR_A(bg) > 0) {
        NvRect bg_rect = { x, y, len * cw, ch };
        draw_fill_rect(surface, &bg_rect, bg);
    }

    int cx = x;
    for (int i = 0; str[i]; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c == '\n') { cx = x; y += ch + 2; continue; }
        if (c == '\t') {
            int tab = 8 * cw;
            cx = ((cx - x) / tab + 1) * tab + x;
            continue;
        }
        int idx = psf_unicode_to_glyph(font, c);
        rasterize_glyph(surface, font, idx, cx, y, fg, scale, bold, italic);
        cx += cw;
    }

    if (underline) {
        draw_hline(surface, x, y + ch, len * cw, fg);
    }
}

void font_measure_string(const char *str, int size, int *out_w, int *out_h) {
    PSFFont *font = font_get_default_psf();
    if (!font) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return;
    }
    int scale = size_to_scale(size);
    int cw    = font->width  * scale;
    int ch    = font->height * scale;

    int max_w = 0, cur_w = 0, lines = 1;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '\n') {
            if (cur_w > max_w) max_w = cur_w;
            cur_w = 0;
            lines++;
        } else {
            cur_w += cw;
        }
    }
    if (cur_w > max_w) max_w = cur_w;

    if (out_w) *out_w = max_w;
    if (out_h) *out_h = lines * (ch + 2);
}

/* -----------------------------------------------------------------------
 * Global init / shutdown
 * --------------------------------------------------------------------- */

void font_init(const char *psf_path) {
    g_psf     = psf_load(psf_path);
    g_default = font_renderer_new(g_psf, 1);
}

void font_shutdown(void) {
    font_renderer_free(g_default);
    psf_free(g_psf);
    g_default = NULL;
    g_psf     = NULL;
}

FontRenderer *font_get_default(void) {
    if (!g_default) font_init(NULL);
    return g_default;
}

PSFFont *font_get_default_psf(void) {
    if (!g_psf) font_init(NULL);
    return g_psf;
}