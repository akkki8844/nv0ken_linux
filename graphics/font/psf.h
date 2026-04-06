#ifndef PSF_H
#define PSF_H

#include <stdint.h>
#include <stddef.h>

/* -----------------------------------------------------------------------
 * PSF1 header
 * --------------------------------------------------------------------- */

#define PSF1_MAGIC0     0x36
#define PSF1_MAGIC1     0x04
#define PSF1_MODE512    0x01
#define PSF1_MODEHASTAB 0x02
#define PSF1_MODEHASSEQ 0x04

typedef struct {
    uint8_t  magic[2];
    uint8_t  mode;
    uint8_t  charsize;
} PSF1Header;

/* -----------------------------------------------------------------------
 * PSF2 header
 * --------------------------------------------------------------------- */

#define PSF2_MAGIC0     0x72
#define PSF2_MAGIC1     0xB5
#define PSF2_MAGIC2     0x4A
#define PSF2_MAGIC3     0x86
#define PSF2_HAS_UNICODE_TABLE 0x01

typedef struct {
    uint8_t  magic[4];
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t num_glyph;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} PSF2Header;

/* -----------------------------------------------------------------------
 * Loaded font
 * --------------------------------------------------------------------- */

typedef enum {
    PSF_VERSION_1,
    PSF_VERSION_2,
} PSFVersion;

typedef struct {
    PSFVersion  version;
    int         width;
    int         height;
    int         bytes_per_glyph;
    int         num_glyphs;
    uint8_t    *glyph_data;
    size_t      glyph_data_size;

    uint16_t   *unicode_table;
    int         unicode_table_size;

    int         owns_data;
} PSFFont;

PSFFont  *psf_load(const char *path);
PSFFont  *psf_load_from_mem(const uint8_t *data, size_t size);
void      psf_free(PSFFont *font);

const uint8_t *psf_glyph(const PSFFont *font, uint32_t codepoint);
int            psf_unicode_to_glyph(const PSFFont *font, uint32_t codepoint);

int  psf_glyph_pixel(const PSFFont *font, int glyph_idx, int x, int y);
int  psf_char_pixel(const PSFFont *font, uint32_t codepoint, int x, int y);

#endif