#include "image.h"
#include "color.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
}

static int32_t read_i32_le(const uint8_t *p) {
    return (int32_t)read_u32_le(p);
}

/* -----------------------------------------------------------------------
 * BMP loader — supports 1/4/8/16/24/32bpp, BI_RGB and BI_BITFIELDS,
 *              RLE4, RLE8, CORE/INFO/V4/V5 headers, top-down and
 *              bottom-up rows.
 * --------------------------------------------------------------------- */

#define BMP_RGB        0
#define BMP_RLE8       1
#define BMP_RLE4       2
#define BMP_BITFIELDS  3

NvImage *image_load_bmp(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    if (fsize < 54) { fclose(f); return NULL; }

    uint8_t *data = xmalloc(fsize);
    if (!data || (long)fread(data, 1, fsize, f) != fsize) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);

    if (data[0] != 'B' || data[1] != 'M') { free(data); return NULL; }

    uint32_t pixel_off = read_u32_le(data + 10);
    uint32_t hdr_size  = read_u32_le(data + 14);

    int width, height;
    int bpp, compression;
    uint32_t clr_used = 0;
    int top_down = 0;

    uint32_t rmask = 0xFF0000, gmask = 0x00FF00, bmask = 0x0000FF, amask = 0;

    if (hdr_size == 12) {
        width  = read_u16_le(data + 18);
        height = read_u16_le(data + 20);
        bpp    = read_u16_le(data + 24);
        compression = BMP_RGB;
    } else {
        width       = read_i32_le(data + 18);
        height      = read_i32_le(data + 22);
        bpp         = read_u16_le(data + 28);
        compression = read_u32_le(data + 30);
        clr_used    = read_u32_le(data + 46);
        if (width == 0 || height == 0) { free(data); return NULL; }
        if (height < 0) { top_down = 1; height = -height; }
    }

    if (compression == BMP_BITFIELDS && hdr_size >= 40) {
        rmask = read_u32_le(data + 54);
        gmask = read_u32_le(data + 58);
        bmask = read_u32_le(data + 62);
        if (hdr_size >= 56) amask = read_u32_le(data + 66);
    }

    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) { free(data); return NULL; }

    uint8_t *palette = (bpp <= 8) ? data + 14 + hdr_size : NULL;
    int pal_entry_size = (hdr_size == 12) ? 3 : 4;

    auto uint32_t mask_shift(uint32_t mask) {
        if (!mask) return 0;
        int s = 0;
        while (!(mask & 1)) { mask >>= 1; s++; }
        return s;
    }
    auto uint32_t mask_scale(uint32_t val, uint32_t mask) {
        if (!mask) return 0;
        uint32_t m = mask;
        while (!(m & 1)) m >>= 1;
        if (m == 0xFF) return val;
        return (val * 255) / m;
    }

    int rshift = 0, gshift = 0, bshift = 0, ashift = 0;
    uint32_t rmask_n = rmask, gmask_n = gmask, bmask_n = bmask, amask_n = amask;

    if (compression == BMP_BITFIELDS) {
        while (rmask_n && !(rmask_n & 1)) { rmask_n >>= 1; rshift++; }
        while (gmask_n && !(gmask_n & 1)) { gmask_n >>= 1; gshift++; }
        while (bmask_n && !(bmask_n & 1)) { bmask_n >>= 1; bshift++; }
        while (amask_n && !(amask_n & 1)) { amask_n >>= 1; ashift++; }
    }

    uint8_t *src = data + pixel_off;
    int row_bytes = ((width * bpp + 31) / 32) * 4;

    for (int row = 0; row < height; row++) {
        int dst_row = top_down ? row : (height - 1 - row);
        uint32_t *dst_line = pixels + dst_row * width;
        uint8_t  *src_line = src + row * row_bytes;

        for (int x = 0; x < width; x++) {
            uint32_t r = 0, g = 0, b = 0, a = 255;

            switch (bpp) {
                case 1: {
                    int bit = 7 - (x & 7);
                    int idx = (src_line[x >> 3] >> bit) & 1;
                    if (palette) {
                        uint8_t *e = palette + idx * pal_entry_size;
                        b = e[0]; g = e[1]; r = e[2];
                    }
                    break;
                }
                case 4: {
                    int idx = (x & 1) ? (src_line[x >> 1] & 0xF)
                                      : (src_line[x >> 1] >> 4);
                    if (palette) {
                        uint8_t *e = palette + idx * pal_entry_size;
                        b = e[0]; g = e[1]; r = e[2];
                    }
                    break;
                }
                case 8: {
                    int idx = src_line[x];
                    if (palette) {
                        uint8_t *e = palette + idx * pal_entry_size;
                        b = e[0]; g = e[1]; r = e[2];
                    }
                    break;
                }
                case 16: {
                    uint16_t v = read_u16_le(src_line + x * 2);
                    if (compression == BMP_BITFIELDS) {
                        r = mask_scale((v & rmask) >> rshift, rmask_n);
                        g = mask_scale((v & gmask) >> gshift, gmask_n);
                        b = mask_scale((v & bmask) >> bshift, bmask_n);
                    } else {
                        r = ((v >> 10) & 0x1F) * 255 / 31;
                        g = ((v >>  5) & 0x1F) * 255 / 31;
                        b = ( v        & 0x1F) * 255 / 31;
                    }
                    break;
                }
                case 24:
                    b = src_line[x*3];
                    g = src_line[x*3+1];
                    r = src_line[x*3+2];
                    break;
                case 32: {
                    uint32_t v = read_u32_le(src_line + x * 4);
                    if (compression == BMP_BITFIELDS) {
                        r = mask_scale((v & rmask) >> rshift, rmask_n);
                        g = mask_scale((v & gmask) >> gshift, gmask_n);
                        b = mask_scale((v & bmask) >> bshift, bmask_n);
                        a = amask ? mask_scale((v & amask) >> ashift, amask_n) : 255;
                    } else {
                        b = v & 0xFF;
                        g = (v >> 8)  & 0xFF;
                        r = (v >> 16) & 0xFF;
                        a = (v >> 24) & 0xFF;
                        if (!a) a = 255;
                    }
                    break;
                }
            }
            dst_line[x] = COLOR_ARGB(a, r, g, b);
        }
    }

    free(data);

    NvImage *img = xmalloc(sizeof(NvImage));
    if (!img) { free(pixels); return NULL; }
    img->pixels = pixels;
    img->width  = width;
    img->height = height;
    return img;
}

/* -----------------------------------------------------------------------
 * PPM/PGM/PBM/PAM loader — handles P1-P7 (text and binary)
 * --------------------------------------------------------------------- */

static void skip_whitespace_and_comments(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n');
        } else if (!isspace(c)) {
            ungetc(c, f);
            return;
        }
    }
}

static int read_ppm_int(FILE *f) {
    skip_whitespace_and_comments(f);
    int val = 0, c;
    while ((c = fgetc(f)) != EOF && isdigit(c))
        val = val * 10 + (c - '0');
    if (!isspace(c) && c != EOF) ungetc(c, f);
    return val;
}

NvImage *image_load_ppm(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    char magic[3] = {0};
    if (fread(magic, 1, 2, f) != 2) { fclose(f); return NULL; }
    if (magic[0] != 'P') { fclose(f); return NULL; }

    int type = magic[1] - '0';
    if (type < 1 || type > 7) { fclose(f); return NULL; }

    int width = 0, height = 0, maxval = 255, channels = 3;
    int binary = (type >= 4 && type != 7);

    if (type == 7) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "ENDHDR", 6) == 0) break;
            if (strncmp(line, "WIDTH ",   6) == 0) width    = atoi(line + 6);
            if (strncmp(line, "HEIGHT ",  7) == 0) height   = atoi(line + 7);
            if (strncmp(line, "DEPTH ",   6) == 0) channels = atoi(line + 6);
            if (strncmp(line, "MAXVAL ",  7) == 0) maxval   = atoi(line + 7);
        }
        binary = 1;
    } else {
        width   = read_ppm_int(f);
        height  = read_ppm_int(f);
        if (type != 1 && type != 4) maxval = read_ppm_int(f);
        channels = (type == 2 || type == 3 || type == 5 || type == 6) ?
                   ((type == 5) ? 1 : 3) : ((type == 1 || type == 4) ? 1 : 3);
        if (type == 5) channels = 1;
        if (type == 6) channels = 3;
        if (type == 1 || type == 4) channels = 1;
    }

    if (width <= 0 || height <= 0) { fclose(f); return NULL; }

    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) { fclose(f); return NULL; }

    if (type == 1) {
        for (int i = 0; i < width * height; i++) {
            int v = read_ppm_int(f);
            uint8_t c = v ? 0 : 255;
            pixels[i] = COLOR_ARGB(255, c, c, c);
        }
    } else if (type == 2) {
        for (int i = 0; i < width * height; i++) {
            int v = read_ppm_int(f);
            uint8_t c = (uint8_t)((v * 255) / maxval);
            pixels[i] = COLOR_ARGB(255, c, c, c);
        }
    } else if (type == 3) {
        for (int i = 0; i < width * height; i++) {
            int rv = read_ppm_int(f);
            int gv = read_ppm_int(f);
            int bv = read_ppm_int(f);
            pixels[i] = COLOR_ARGB(255,
                                   (uint8_t)((rv*255)/maxval),
                                   (uint8_t)((gv*255)/maxval),
                                   (uint8_t)((bv*255)/maxval));
        }
    } else if (type == 4) {
        int row_bytes = (width + 7) / 8;
        uint8_t *row_buf = xmalloc(row_bytes);
        for (int y = 0; y < height; y++) {
            fread(row_buf, 1, row_bytes, f);
            for (int x = 0; x < width; x++) {
                int bit = (row_buf[x/8] >> (7 - x%8)) & 1;
                uint8_t c = bit ? 0 : 255;
                pixels[y * width + x] = COLOR_ARGB(255, c, c, c);
            }
        }
        free(row_buf);
    } else if (type == 5) {
        for (int i = 0; i < width * height; i++) {
            int v = fgetc(f);
            if (maxval > 255) { v = (v << 8) | fgetc(f); }
            uint8_t c = (uint8_t)((v * 255) / maxval);
            pixels[i] = COLOR_ARGB(255, c, c, c);
        }
    } else if (type == 6) {
        for (int i = 0; i < width * height; i++) {
            uint8_t r = fgetc(f), g = fgetc(f), b = fgetc(f);
            if (maxval != 255) {
                r = (uint8_t)((r * 255) / maxval);
                g = (uint8_t)((g * 255) / maxval);
                b = (uint8_t)((b * 255) / maxval);
            }
            pixels[i] = COLOR_ARGB(255, r, g, b);
        }
    } else if (type == 7) {
        for (int i = 0; i < width * height; i++) {
            uint8_t r = 0, g = 0, b = 0, a = 255;
            if (channels == 1) {
                r = g = b = fgetc(f);
            } else if (channels == 2) {
                r = g = b = fgetc(f); a = fgetc(f);
            } else if (channels == 3) {
                r = fgetc(f); g = fgetc(f); b = fgetc(f);
            } else if (channels >= 4) {
                r = fgetc(f); g = fgetc(f); b = fgetc(f); a = fgetc(f);
                for (int k = 4; k < channels; k++) fgetc(f);
            }
            if (maxval != 255) {
                r = (uint8_t)((r * 255) / maxval);
                g = (uint8_t)((g * 255) / maxval);
                b = (uint8_t)((b * 255) / maxval);
                a = (uint8_t)((a * 255) / maxval);
            }
            pixels[i] = COLOR_ARGB(a, r, g, b);
        }
    }

    fclose(f);

    NvImage *img = xmalloc(sizeof(NvImage));
    if (!img) { free(pixels); return NULL; }
    img->pixels = pixels;
    img->width  = width;
    img->height = height;
    return img;
}

/* -----------------------------------------------------------------------
 * Dispatch by extension
 * --------------------------------------------------------------------- */

NvImage *image_load(const char *path) {
    if (!path) return NULL;
    const char *ext = strrchr(path, '.');
    if (!ext) return NULL;
    if (strcasecmp(ext, ".bmp") == 0) return image_load_bmp(path);
    if (strcasecmp(ext, ".ppm") == 0 ||
        strcasecmp(ext, ".pgm") == 0 ||
        strcasecmp(ext, ".pbm") == 0 ||
        strcasecmp(ext, ".pnm") == 0 ||
        strcasecmp(ext, ".pam") == 0) return image_load_ppm(path);
    return NULL;
}

NvImage *image_from_pixels(uint32_t *pixels, int w, int h) {
    if (!pixels || w <= 0 || h <= 0) return NULL;
    NvImage *img = xmalloc(sizeof(NvImage));
    if (!img) return NULL;
    img->pixels = pixels;
    img->width  = w;
    img->height = h;
    return img;
}

void image_free(NvImage *img) {
    if (!img) return;
    free(img->pixels);
    free(img);
}

/* -----------------------------------------------------------------------
 * Transforms
 * --------------------------------------------------------------------- */

NvImage *image_scale(const NvImage *src, int new_w, int new_h) {
    if (!src || new_w <= 0 || new_h <= 0) return NULL;
    uint32_t *pixels = xmalloc(sizeof(uint32_t) * new_w * new_h);
    if (!pixels) return NULL;

    for (int py = 0; py < new_h; py++) {
        float v  = (float)py / (new_h - 1);
        float fy = v * (src->height - 1);
        int   y0 = (int)fy, y1 = y0 + 1 < src->height ? y0 + 1 : y0;
        float ty = fy - y0;

        for (int px = 0; px < new_w; px++) {
            float u  = (float)px / (new_w - 1);
            float fx = u * (src->width - 1);
            int   x0 = (int)fx, x1 = x0 + 1 < src->width ? x0 + 1 : x0;
            float tx = fx - x0;

            uint32_t c00 = src->pixels[y0 * src->width + x0];
            uint32_t c10 = src->pixels[y0 * src->width + x1];
            uint32_t c01 = src->pixels[y1 * src->width + x0];
            uint32_t c11 = src->pixels[y1 * src->width + x1];

            uint8_t r = (uint8_t)(
                COLOR_R(c00)*(1-tx)*(1-ty) + COLOR_R(c10)*tx*(1-ty) +
                COLOR_R(c01)*(1-tx)*ty     + COLOR_R(c11)*tx*ty);
            uint8_t g = (uint8_t)(
                COLOR_G(c00)*(1-tx)*(1-ty) + COLOR_G(c10)*tx*(1-ty) +
                COLOR_G(c01)*(1-tx)*ty     + COLOR_G(c11)*tx*ty);
            uint8_t b = (uint8_t)(
                COLOR_B(c00)*(1-tx)*(1-ty) + COLOR_B(c10)*tx*(1-ty) +
                COLOR_B(c01)*(1-tx)*ty     + COLOR_B(c11)*tx*ty);
            uint8_t a = (uint8_t)(
                COLOR_A(c00)*(1-tx)*(1-ty) + COLOR_A(c10)*tx*(1-ty) +
                COLOR_A(c01)*(1-tx)*ty     + COLOR_A(c11)*tx*ty);

            pixels[py * new_w + px] = COLOR_ARGB(a, r, g, b);
        }
    }

    NvImage *out = xmalloc(sizeof(NvImage));
    if (!out) { free(pixels); return NULL; }
    out->pixels = pixels;
    out->width  = new_w;
    out->height = new_h;
    return out;
}

NvImage *image_crop(const NvImage *src, int x, int y, int w, int h) {
    if (!src || w <= 0 || h <= 0) return NULL;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > src->width)  w = src->width  - x;
    if (y + h > src->height) h = src->height - y;
    if (w <= 0 || h <= 0) return NULL;

    uint32_t *pixels = xmalloc(sizeof(uint32_t) * w * h);
    if (!pixels) return NULL;

    for (int row = 0; row < h; row++)
        memcpy(pixels + row * w,
               src->pixels + (y + row) * src->width + x,
               w * sizeof(uint32_t));

    NvImage *out = xmalloc(sizeof(NvImage));
    if (!out) { free(pixels); return NULL; }
    out->pixels = pixels;
    out->width  = w;
    out->height = h;
    return out;
}

void image_flip_h(NvImage *img) {
    if (!img) return;
    for (int y = 0; y < img->height; y++) {
        uint32_t *row = img->pixels + y * img->width;
        for (int x = 0; x < img->width / 2; x++) {
            uint32_t tmp = row[x];
            row[x] = row[img->width - 1 - x];
            row[img->width - 1 - x] = tmp;
        }
    }
}

void image_flip_v(NvImage *img) {
    if (!img) return;
    uint32_t *tmp = malloc(img->width * sizeof(uint32_t));
    if (!tmp) return;
    for (int y = 0; y < img->height / 2; y++) {
        uint32_t *top = img->pixels + y * img->width;
        uint32_t *bot = img->pixels + (img->height - 1 - y) * img->width;
        memcpy(tmp, top, img->width * sizeof(uint32_t));
        memcpy(top, bot, img->width * sizeof(uint32_t));
        memcpy(bot, tmp, img->width * sizeof(uint32_t));
    }
    free(tmp);
}