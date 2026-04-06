#ifndef FONT_RENDER_H
#define FONT_RENDER_H

#include "../draw/draw.h"
#include "psf.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Font renderer — wraps a PSFFont and provides high-level draw calls.
 * All functions work directly on NvSurface with no external state.
 * --------------------------------------------------------------------- */

typedef struct {
    PSFFont  *font;
    int       scale;
    int       glyph_w;
    int       glyph_h;
    int       line_height;
    int       tab_width;
} FontRenderer;

FontRenderer *font_renderer_new(PSFFont *font, int scale);
void          font_renderer_free(FontRenderer *fr);
void          font_renderer_set_font(FontRenderer *fr, PSFFont *font);
void          font_renderer_set_scale(FontRenderer *fr, int scale);

int           font_glyph_width(const FontRenderer *fr);
int           font_glyph_height(const FontRenderer *fr);
int           font_line_height(const FontRenderer *fr);
int           font_string_width(const FontRenderer *fr, const char *str);
int           font_string_width_n(const FontRenderer *fr,
                                  const char *str, int n);

void font_draw_char(NvSurface *surface, int x, int y,
                    char c, uint32_t color,
                    int size, int bold);

void font_draw_string(NvSurface *surface, int x, int y,
                      const char *str, uint32_t color, int size);

void font_draw_string_clip(NvSurface *surface, int x, int y,
                            const char *str, uint32_t color,
                            int size, int max_w);

void font_draw_string_styled(NvSurface *surface, int x, int y,
                              const char *str, uint32_t fg,
                              uint32_t bg, int size,
                              int bold, int italic, int underline);

void font_draw_char_scaled(NvSurface *surface, int x, int y,
                            uint32_t codepoint, uint32_t color,
                            int scale, const PSFFont *font);

void font_measure_string(const char *str, int size,
                          int *out_w, int *out_h);

/* -----------------------------------------------------------------------
 * Global default font — used by font_draw_* when no renderer is provided.
 * Call font_init() once at startup.
 * --------------------------------------------------------------------- */

void          font_init(const char *psf_path);
void          font_shutdown(void);
FontRenderer *font_get_default(void);
PSFFont      *font_get_default_psf(void);

#endif