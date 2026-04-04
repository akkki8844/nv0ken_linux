#include "decode_ppm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define MAX_PPM_DIM     16384
#define MAX_HEADER_LEN  256

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

/* -----------------------------------------------------------------------
 * Parser state for reading PPM from a memory buffer
 * --------------------------------------------------------------------- */

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         pos;
} PpmReader;

static int reader_eof(PpmReader *r) {
    return r->pos >= r->len;
}

static uint8_t reader_byte(PpmReader *r) {
    if (r->pos >= r->len) return 0;
    return r->data[r->pos++];
}

static void reader_skip_whitespace_and_comments(PpmReader *r) {
    while (!reader_eof(r)) {
        uint8_t c = r->data[r->pos];
        if (c == '#') {
            while (!reader_eof(r) && r->data[r->pos] != '\n')
                r->pos++;
        } else if (isspace(c)) {
            r->pos++;
        } else {
            break;
        }
    }
}

static int reader_read_uint(PpmReader *r, unsigned int *out) {
    reader_skip_whitespace_and_comments(r);
    if (reader_eof(r)) return -1;

    uint8_t c = r->data[r->pos];
    if (!isdigit(c)) return -1;

    unsigned int val = 0;
    while (!reader_eof(r) && isdigit(r->data[r->pos])) {
        val = val * 10 + (r->data[r->pos] - '0');
        r->pos++;
    }
    *out = val;
    return 0;
}

static int reader_read_magic(PpmReader *r, char *out) {
    if (r->pos + 2 > r->len) return -1;
    out[0] = (char)r->data[r->pos++];
    out[1] = (char)r->data[r->pos++];
    out[2] = '\0';
    return 0;
}

/* -----------------------------------------------------------------------
 * Channel scale helper
 * --------------------------------------------------------------------- */

static uint8_t scale_channel(unsigned int val, unsigned int maxval) {
    if (maxval == 255) return (uint8_t)val;
    if (maxval == 0)   return 0;
    return (uint8_t)((val * 255 + maxval / 2) / maxval);
}

/* -----------------------------------------------------------------------
 * P1 — plain PBM (ASCII bitmap)
 * --------------------------------------------------------------------- */

static PpmImage *decode_p1(PpmReader *r, int width, int height) {
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    for (int i = 0; i < width * height; i++) {
        unsigned int bit;
        if (reader_read_uint(r, &bit) < 0) bit = 0;
        pixels[i] = bit ? 0xFF000000 : 0xFFFFFFFF;
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PBM;
    return img;
}

/* -----------------------------------------------------------------------
 * P2 — plain PGM (ASCII grayscale)
 * --------------------------------------------------------------------- */

static PpmImage *decode_p2(PpmReader *r, int width, int height,
                            unsigned int maxval) {
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    for (int i = 0; i < width * height; i++) {
        unsigned int v;
        if (reader_read_uint(r, &v) < 0) v = 0;
        uint8_t g = scale_channel(v, maxval);
        pixels[i] = 0xFF000000 | ((uint32_t)g << 16) |
                    ((uint32_t)g << 8) | (uint32_t)g;
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PGM;
    return img;
}

/* -----------------------------------------------------------------------
 * P3 — plain PPM (ASCII RGB)
 * --------------------------------------------------------------------- */

static PpmImage *decode_p3(PpmReader *r, int width, int height,
                            unsigned int maxval) {
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    for (int i = 0; i < width * height; i++) {
        unsigned int rv, gv, bv;
        if (reader_read_uint(r, &rv) < 0) rv = 0;
        if (reader_read_uint(r, &gv) < 0) gv = 0;
        if (reader_read_uint(r, &bv) < 0) bv = 0;
        uint8_t r8 = scale_channel(rv, maxval);
        uint8_t g8 = scale_channel(gv, maxval);
        uint8_t b8 = scale_channel(bv, maxval);
        pixels[i] = 0xFF000000 | ((uint32_t)r8 << 16) |
                    ((uint32_t)g8 << 8) | (uint32_t)b8;
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PPM;
    return img;
}

/* -----------------------------------------------------------------------
 * P4 — binary PBM
 * --------------------------------------------------------------------- */

static PpmImage *decode_p4(PpmReader *r, int width, int height) {
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    int row_bytes = (width + 7) / 8;

    for (int y = 0; y < height; y++) {
        for (int bx = 0; bx < row_bytes; bx++) {
            uint8_t byte = reader_eof(r) ? 0 : reader_byte(r);
            for (int bit = 7; bit >= 0; bit--) {
                int x = bx * 8 + (7 - bit);
                if (x >= width) break;
                int set = (byte >> bit) & 1;
                pixels[y * width + x] = set ? 0xFF000000 : 0xFFFFFFFF;
            }
        }
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PBM;
    return img;
}

/* -----------------------------------------------------------------------
 * P5 — binary PGM
 * --------------------------------------------------------------------- */

static PpmImage *decode_p5(PpmReader *r, int width, int height,
                            unsigned int maxval) {
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    int is_16bit = maxval > 255;

    for (int i = 0; i < width * height; i++) {
        unsigned int v;
        if (is_16bit) {
            uint8_t hi = reader_eof(r) ? 0 : reader_byte(r);
            uint8_t lo = reader_eof(r) ? 0 : reader_byte(r);
            v = ((unsigned int)hi << 8) | lo;
        } else {
            v = reader_eof(r) ? 0 : reader_byte(r);
        }
        uint8_t g = scale_channel(v, maxval);
        pixels[i] = 0xFF000000 | ((uint32_t)g << 16) |
                    ((uint32_t)g << 8) | (uint32_t)g;
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PGM;
    return img;
}

/* -----------------------------------------------------------------------
 * P6 — binary PPM
 * --------------------------------------------------------------------- */

static PpmImage *decode_p6(PpmReader *r, int width, int height,
                            unsigned int maxval) {
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    int is_16bit = maxval > 255;

    for (int i = 0; i < width * height; i++) {
        unsigned int rv, gv, bv;
        if (is_16bit) {
            uint8_t rhi = reader_eof(r) ? 0 : reader_byte(r);
            uint8_t rlo = reader_eof(r) ? 0 : reader_byte(r);
            uint8_t ghi = reader_eof(r) ? 0 : reader_byte(r);
            uint8_t glo = reader_eof(r) ? 0 : reader_byte(r);
            uint8_t bhi = reader_eof(r) ? 0 : reader_byte(r);
            uint8_t blo = reader_eof(r) ? 0 : reader_byte(r);
            rv = ((unsigned int)rhi << 8) | rlo;
            gv = ((unsigned int)ghi << 8) | glo;
            bv = ((unsigned int)bhi << 8) | blo;
        } else {
            rv = reader_eof(r) ? 0 : reader_byte(r);
            gv = reader_eof(r) ? 0 : reader_byte(r);
            bv = reader_eof(r) ? 0 : reader_byte(r);
        }
        uint8_t r8 = scale_channel(rv, maxval);
        uint8_t g8 = scale_channel(gv, maxval);
        uint8_t b8 = scale_channel(bv, maxval);
        pixels[i] = 0xFF000000 | ((uint32_t)r8 << 16) |
                    ((uint32_t)g8 << 8) | (uint32_t)b8;
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PPM;
    return img;
}

/* -----------------------------------------------------------------------
 * P7 — PAM (Portable Arbitrary Map)
 * --------------------------------------------------------------------- */

static PpmImage *decode_p7(PpmReader *r, size_t total_len) {
    int      width    = 0;
    int      height   = 0;
    int      depth    = 0;
    unsigned maxval   = 255;
    char     tupltype[64] = "RGB";

    while (!reader_eof(r)) {
        char line[128] = {0};
        int li = 0;
        while (!reader_eof(r) && li < 127) {
            char c = (char)reader_byte(r);
            if (c == '\n') break;
            line[li++] = c;
        }
        line[li] = '\0';

        if (strncmp(line, "ENDHDR", 6) == 0) break;
        if (strncmp(line, "WIDTH ",  6) == 0) width  = atoi(line + 6);
        if (strncmp(line, "HEIGHT ", 7) == 0) height = atoi(line + 7);
        if (strncmp(line, "DEPTH ",  6) == 0) depth  = atoi(line + 6);
        if (strncmp(line, "MAXVAL ", 7) == 0) maxval = (unsigned)atoi(line + 7);
        if (strncmp(line, "TUPLTYPE ", 9) == 0)
            strncpy(tupltype, line + 9, sizeof(tupltype) - 1);
    }

    if (width <= 0 || height <= 0 || depth <= 0 ||
        width > MAX_PPM_DIM || height > MAX_PPM_DIM) return NULL;

    int is_16bit = maxval > 255;
    int bytes_per_channel = is_16bit ? 2 : 1;
    int has_alpha = (depth == 2 || depth == 4) ||
                    strstr(tupltype, "ALPHA") != NULL;

    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) return NULL;

    for (int i = 0; i < width * height; i++) {
        unsigned int chans[4] = {0, 0, 0, 255};
        for (int d = 0; d < depth && d < 4; d++) {
            if (is_16bit) {
                uint8_t hi = reader_eof(r) ? 0 : reader_byte(r);
                uint8_t lo = reader_eof(r) ? 0 : reader_byte(r);
                chans[d] = ((unsigned int)hi << 8) | lo;
            } else {
                chans[d] = reader_eof(r) ? 0 : reader_byte(r);
            }
        }

        uint8_t r8, g8, b8, a8;

        if (depth == 1) {
            uint8_t gray = scale_channel(chans[0], maxval);
            r8 = g8 = b8 = gray;
            a8 = 0xFF;
        } else if (depth == 2) {
            uint8_t gray = scale_channel(chans[0], maxval);
            r8 = g8 = b8 = gray;
            a8 = scale_channel(chans[1], maxval);
        } else if (depth == 3) {
            r8 = scale_channel(chans[0], maxval);
            g8 = scale_channel(chans[1], maxval);
            b8 = scale_channel(chans[2], maxval);
            a8 = 0xFF;
        } else {
            r8 = scale_channel(chans[0], maxval);
            g8 = scale_channel(chans[1], maxval);
            b8 = scale_channel(chans[2], maxval);
            a8 = scale_channel(chans[3], maxval);
        }

        pixels[i] = ((uint32_t)a8 << 24) | ((uint32_t)r8 << 16) |
                    ((uint32_t)g8 << 8)  |  (uint32_t)b8;
    }

    PpmImage *img = xmalloc(sizeof(PpmImage));
    if (!img) { free(pixels); return NULL; }
    img->width   = width;
    img->height  = height;
    img->pixels  = pixels;
    img->format  = PPM_FORMAT_PAM;
    return img;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

PpmImage *ppm_decode(const uint8_t *data, size_t data_len) {
    if (!data || data_len < 3) return NULL;

    PpmReader r;
    r.data = data;
    r.len  = data_len;
    r.pos  = 0;

    char magic[3];
    if (reader_read_magic(&r, magic) < 0) return NULL;
    if (magic[0] != 'P') return NULL;

    if (magic[1] == '7') {
        if (r.data[r.pos] != '\n') {
            while (!reader_eof(&r) && r.data[r.pos] != '\n') r.pos++;
        }
        r.pos++;
        return decode_p7(&r, data_len);
    }

    unsigned int width = 0, height = 0, maxval = 1;

    if (reader_read_uint(&r, &width)  < 0) return NULL;
    if (reader_read_uint(&r, &height) < 0) return NULL;

    if (magic[1] != '1' && magic[1] != '4') {
        if (reader_read_uint(&r, &maxval) < 0) return NULL;
        if (maxval == 0 || maxval > 65535)      return NULL;
    }

    if ((int)width <= 0 || (int)height <= 0) return NULL;
    if ((int)width > MAX_PPM_DIM || (int)height > MAX_PPM_DIM) return NULL;

    if (magic[1] != '1' && magic[1] != '4') {
        if (!reader_eof(&r) && isspace(r.data[r.pos])) r.pos++;
    } else {
        reader_skip_whitespace_and_comments(&r);
    }

    switch (magic[1]) {
        case '1': return decode_p1(&r, (int)width, (int)height);
        case '2': return decode_p2(&r, (int)width, (int)height, maxval);
        case '3': return decode_p3(&r, (int)width, (int)height, maxval);
        case '4': return decode_p4(&r, (int)width, (int)height);
        case '5': return decode_p5(&r, (int)width, (int)height, maxval);
        case '6': return decode_p6(&r, (int)width, (int)height, maxval);
        default:  return NULL;
    }
}

PpmImage *ppm_decode_file(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 256 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }

    uint8_t *data = xmalloc((size_t)file_size);
    if (!data) { fclose(f); return NULL; }

    size_t n = fread(data, 1, (size_t)file_size, f);
    fclose(f);

    if ((long)n != file_size) { free(data); return NULL; }

    PpmImage *img = ppm_decode(data, (size_t)file_size);
    free(data);
    return img;
}

void ppm_free(PpmImage *img) {
    if (!img) return;
    free(img->pixels);
    free(img);
}

uint32_t ppm_get_pixel(const PpmImage *img, int x, int y) {
    if (!img || x < 0 || y < 0 || x >= img->width || y >= img->height)
        return 0;
    return img->pixels[y * img->width + x];
}

const char *ppm_format_name(PpmFormat fmt) {
    switch (fmt) {
        case PPM_FORMAT_PBM: return "PBM";
        case PPM_FORMAT_PGM: return "PGM";
        case PPM_FORMAT_PPM: return "PPM";
        case PPM_FORMAT_PAM: return "PAM";
        default:             return "unknown";
    }
}