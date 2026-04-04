#include "decode_bmp.h"
#include <stdlib.h>
#include <string.h>

#define BMP_SIGNATURE       0x4D42
#define BI_RGB              0
#define BI_RLE8             1
#define BI_RLE4             2
#define BI_BITFIELDS        3
#define BI_ALPHABITFIELDS   6

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

/* -----------------------------------------------------------------------
 * Little-endian read helpers
 * --------------------------------------------------------------------- */

static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static int32_t read_i32(const uint8_t *p) {
    return (int32_t)read_u32(p);
}

/* -----------------------------------------------------------------------
 * BMP file header (14 bytes)
 * --------------------------------------------------------------------- */

typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint32_t reserved;
    uint32_t pixel_offset;
} BmpFileHeader;

/* -----------------------------------------------------------------------
 * DIB header — supports BITMAPCOREHEADER (12), BITMAPINFOHEADER (40),
 * BITMAPV4HEADER (108), BITMAPV5HEADER (124)
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pels_per_meter;
    int32_t  y_pels_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t alpha_mask;
} DibHeader;

/* -----------------------------------------------------------------------
 * Bit-mask helpers for 16/32-bit modes
 * --------------------------------------------------------------------- */

static int mask_shift(uint32_t mask) {
    if (!mask) return 0;
    int shift = 0;
    while (!(mask & 1)) { mask >>= 1; shift++; }
    return shift;
}

static int mask_bits(uint32_t mask) {
    int bits = 0;
    while (mask) { bits += mask & 1; mask >>= 1; }
    return bits;
}

static uint8_t extract_channel(uint32_t pixel, uint32_t mask) {
    if (!mask) return 0xFF;
    int shift = mask_shift(mask);
    int bits  = mask_bits(mask);
    uint32_t val = (pixel & mask) >> shift;
    if (bits >= 8) return (uint8_t)(val >> (bits - 8));
    if (bits == 0) return 0;
    return (uint8_t)((val * 255) / ((1u << bits) - 1));
}

/* -----------------------------------------------------------------------
 * Palette
 * --------------------------------------------------------------------- */

static uint32_t *read_palette(const uint8_t *data, size_t data_len,
                               size_t palette_offset,
                               int color_count, int quad_size) {
    if (color_count <= 0 || color_count > 256) return NULL;
    uint32_t *pal = xmalloc(sizeof(uint32_t) * color_count);
    if (!pal) return NULL;

    for (int i = 0; i < color_count; i++) {
        size_t off = palette_offset + i * quad_size;
        if (off + quad_size > data_len) {
            pal[i] = 0xFF000000;
            continue;
        }
        uint8_t b = data[off + 0];
        uint8_t g = data[off + 1];
        uint8_t r = data[off + 2];
        uint8_t a = quad_size == 4 ? data[off + 3] : 0xFF;
        pal[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                 ((uint32_t)g << 8)  |  (uint32_t)b;
    }
    return pal;
}

/* -----------------------------------------------------------------------
 * Row decoders
 * --------------------------------------------------------------------- */

static void decode_row_1bpp(const uint8_t *row_data, uint32_t *dst,
                              int width, uint32_t *palette) {
    for (int x = 0; x < width; x++) {
        int byte_idx = x / 8;
        int bit_idx  = 7 - (x % 8);
        int idx = (row_data[byte_idx] >> bit_idx) & 1;
        dst[x] = palette ? palette[idx] : (idx ? 0xFFFFFFFF : 0xFF000000);
    }
}

static void decode_row_4bpp(const uint8_t *row_data, uint32_t *dst,
                              int width, uint32_t *palette) {
    for (int x = 0; x < width; x++) {
        int byte_idx = x / 2;
        int idx = (x % 2 == 0)
                  ? (row_data[byte_idx] >> 4) & 0x0F
                  : row_data[byte_idx] & 0x0F;
        dst[x] = palette ? palette[idx & 15] : 0xFF000000;
    }
}

static void decode_row_8bpp(const uint8_t *row_data, uint32_t *dst,
                              int width, uint32_t *palette) {
    for (int x = 0; x < width; x++) {
        int idx = row_data[x];
        dst[x] = palette ? palette[idx] : 0xFF000000;
    }
}

static void decode_row_16bpp(const uint8_t *row_data, uint32_t *dst,
                               int width, uint32_t r_mask, uint32_t g_mask,
                               uint32_t b_mask, uint32_t a_mask) {
    for (int x = 0; x < width; x++) {
        uint16_t px = read_u16(row_data + x * 2);
        uint8_t r = extract_channel(px, r_mask);
        uint8_t g = extract_channel(px, g_mask);
        uint8_t b = extract_channel(px, b_mask);
        uint8_t a = a_mask ? extract_channel(px, a_mask) : 0xFF;
        dst[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                 ((uint32_t)g << 8)  |  (uint32_t)b;
    }
}

static void decode_row_24bpp(const uint8_t *row_data, uint32_t *dst,
                               int width) {
    for (int x = 0; x < width; x++) {
        uint8_t b = row_data[x * 3 + 0];
        uint8_t g = row_data[x * 3 + 1];
        uint8_t r = row_data[x * 3 + 2];
        dst[x] = 0xFF000000 | ((uint32_t)r << 16) |
                 ((uint32_t)g << 8) | (uint32_t)b;
    }
}

static void decode_row_32bpp(const uint8_t *row_data, uint32_t *dst,
                               int width, uint32_t r_mask, uint32_t g_mask,
                               uint32_t b_mask, uint32_t a_mask) {
    for (int x = 0; x < width; x++) {
        uint32_t px = read_u32(row_data + x * 4);
        if (!r_mask && !g_mask && !b_mask) {
            uint8_t b = (px >>  0) & 0xFF;
            uint8_t g = (px >>  8) & 0xFF;
            uint8_t r = (px >> 16) & 0xFF;
            uint8_t a = (px >> 24) & 0xFF;
            if (!a) a = 0xFF;
            dst[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                     ((uint32_t)g << 8)  |  (uint32_t)b;
        } else {
            uint8_t r = extract_channel(px, r_mask);
            uint8_t g = extract_channel(px, g_mask);
            uint8_t b = extract_channel(px, b_mask);
            uint8_t a = a_mask ? extract_channel(px, a_mask) : 0xFF;
            dst[x] = ((uint32_t)a << 24) | ((uint32_t)r << 16) |
                     ((uint32_t)g << 8)  |  (uint32_t)b;
        }
    }
}

/* -----------------------------------------------------------------------
 * RLE4 decoder
 * --------------------------------------------------------------------- */

static int decode_rle4(const uint8_t *src, size_t src_len,
                        uint32_t *dst, int width, int height,
                        uint32_t *palette) {
    int x = 0, y = height - 1;
    size_t i = 0;

    while (i < src_len && y >= 0) {
        uint8_t b0 = src[i++];
        if (i >= src_len) break;
        uint8_t b1 = src[i++];

        if (b0 == 0) {
            if (b1 == 0) {
                x = 0; y--;
            } else if (b1 == 1) {
                break;
            } else if (b1 == 2) {
                if (i + 2 > src_len) break;
                x += src[i++];
                y -= src[i++];
            } else {
                int count = b1;
                int bytes = (count + 1) / 2;
                int pad   = (bytes % 2 != 0) ? 1 : 0;
                for (int k = 0; k < count && i < src_len; k++) {
                    uint8_t byte = src[i + k / 2];
                    int idx = (k % 2 == 0) ? (byte >> 4) & 0x0F : byte & 0x0F;
                    if (x < width && y >= 0 && y < height)
                        dst[y * width + x] = palette ? palette[idx] : 0xFF000000;
                    x++;
                }
                i += bytes + pad;
            }
        } else {
            int count = b0;
            int hi = (b1 >> 4) & 0x0F;
            int lo = b1 & 0x0F;
            for (int k = 0; k < count; k++) {
                int idx = (k % 2 == 0) ? hi : lo;
                if (x < width && y >= 0 && y < height)
                    dst[y * width + x] = palette ? palette[idx] : 0xFF000000;
                x++;
            }
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * RLE8 decoder
 * --------------------------------------------------------------------- */

static int decode_rle8(const uint8_t *src, size_t src_len,
                        uint32_t *dst, int width, int height,
                        uint32_t *palette) {
    int x = 0, y = height - 1;
    size_t i = 0;

    while (i < src_len && y >= 0) {
        uint8_t b0 = src[i++];
        if (i >= src_len) break;
        uint8_t b1 = src[i++];

        if (b0 == 0) {
            if (b1 == 0) {
                x = 0; y--;
            } else if (b1 == 1) {
                break;
            } else if (b1 == 2) {
                if (i + 2 > src_len) break;
                x += src[i++];
                y -= src[i++];
            } else {
                int count = b1;
                int pad   = count % 2;
                for (int k = 0; k < count && i < src_len; k++, i++) {
                    int idx = src[i];
                    if (x < width && y >= 0 && y < height)
                        dst[y * width + x] = palette ? palette[idx & 255] : 0xFF000000;
                    x++;
                }
                if (pad) i++;
            }
        } else {
            int count = b0;
            int idx   = b1;
            uint32_t color = palette ? palette[idx & 255] : 0xFF000000;
            for (int k = 0; k < count; k++) {
                if (x < width && y >= 0 && y < height)
                    dst[y * width + x] = color;
                x++;
            }
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

BmpImage *bmp_decode(const uint8_t *data, size_t data_len) {
    if (!data || data_len < 14) return NULL;

    BmpFileHeader fh;
    fh.signature   = read_u16(data + 0);
    fh.file_size   = read_u32(data + 2);
    fh.pixel_offset= read_u32(data + 10);

    if (fh.signature != BMP_SIGNATURE) return NULL;
    if (fh.pixel_offset >= data_len)   return NULL;

    if (data_len < 18) return NULL;
    DibHeader dib;
    memset(&dib, 0, sizeof(dib));
    dib.header_size = read_u32(data + 14);

    int is_core = (dib.header_size == 12);

    if (is_core) {
        if (data_len < 14 + 12) return NULL;
        dib.width     = (int32_t)(int16_t)read_u16(data + 18);
        dib.height    = (int32_t)(int16_t)read_u16(data + 20);
        dib.planes    = read_u16(data + 22);
        dib.bit_count = read_u16(data + 24);
        dib.compression = BI_RGB;
    } else {
        if (data_len < 14 + 40) return NULL;
        dib.width          = read_i32(data + 18);
        dib.height         = read_i32(data + 22);
        dib.planes         = read_u16(data + 26);
        dib.bit_count      = read_u16(data + 28);
        dib.compression    = read_u32(data + 30);
        dib.image_size     = read_u32(data + 34);
        dib.x_pels_per_meter = read_i32(data + 38);
        dib.y_pels_per_meter = read_i32(data + 42);
        dib.clr_used       = read_u32(data + 46);
        dib.clr_important  = read_u32(data + 50);

        if (dib.header_size >= 52 && data_len >= 14 + 52) {
            dib.red_mask   = read_u32(data + 54);
            dib.green_mask = read_u32(data + 58);
            dib.blue_mask  = read_u32(data + 62);
        }
        if (dib.header_size >= 56 && data_len >= 14 + 56) {
            dib.alpha_mask = read_u32(data + 66);
        }

        if (dib.compression == BI_BITFIELDS && data_len >= 14 + 40 + 12) {
            if (!dib.red_mask)   dib.red_mask   = read_u32(data + 54);
            if (!dib.green_mask) dib.green_mask = read_u32(data + 58);
            if (!dib.blue_mask)  dib.blue_mask  = read_u32(data + 62);
        }
        if (dib.compression == BI_ALPHABITFIELDS && data_len >= 14 + 40 + 16) {
            if (!dib.alpha_mask) dib.alpha_mask = read_u32(data + 66);
        }
    }

    int width  = dib.width;
    int height = dib.height;
    int flip   = 1;

    if (height < 0) { height = -height; flip = 0; }
    if (width <= 0 || height <= 0) return NULL;
    if (width > 16384 || height > 16384) return NULL;

    int bit_count = dib.bit_count;
    if (bit_count != 1 && bit_count != 4 && bit_count != 8 &&
        bit_count != 16 && bit_count != 24 && bit_count != 32)
        return NULL;

    int palette_offset = 14 + (int)dib.header_size;
    int palette_count  = 0;
    int quad_size      = is_core ? 3 : 4;

    if (bit_count <= 8) {
        palette_count = dib.clr_used > 0
                        ? (int)dib.clr_used
                        : (1 << bit_count);
        if (palette_count > 256) palette_count = 256;
    }

    if (dib.compression == BI_BITFIELDS || dib.compression == BI_ALPHABITFIELDS) {
        if (!dib.red_mask) {
            if (bit_count == 16) {
                dib.red_mask   = 0x7C00;
                dib.green_mask = 0x03E0;
                dib.blue_mask  = 0x001F;
            } else {
                dib.red_mask   = 0x00FF0000;
                dib.green_mask = 0x0000FF00;
                dib.blue_mask  = 0x000000FF;
            }
        }
    } else if (dib.compression == BI_RGB) {
        if (bit_count == 16) {
            dib.red_mask   = 0x7C00;
            dib.green_mask = 0x03E0;
            dib.blue_mask  = 0x001F;
            dib.alpha_mask = 0;
        }
    }

    uint32_t *palette = NULL;
    if (palette_count > 0) {
        palette = read_palette(data, data_len,
                               palette_offset, palette_count, quad_size);
    }

    uint32_t *pixels = xmalloc(sizeof(uint32_t) * width * height);
    if (!pixels) { free(palette); return NULL; }
    memset(pixels, 0xFF, sizeof(uint32_t) * width * height);

    if (dib.compression == BI_RLE8) {
        const uint8_t *rle_data = data + fh.pixel_offset;
        size_t rle_len = data_len - fh.pixel_offset;
        decode_rle8(rle_data, rle_len, pixels, width, height, palette);
        free(palette);
        goto done;
    }

    if (dib.compression == BI_RLE4) {
        const uint8_t *rle_data = data + fh.pixel_offset;
        size_t rle_len = data_len - fh.pixel_offset;
        decode_rle4(rle_data, rle_len, pixels, width, height, palette);
        free(palette);
        goto done;
    }

    int row_bytes_raw = (width * bit_count + 7) / 8;
    int row_stride    = (row_bytes_raw + 3) & ~3;

    for (int row = 0; row < height; row++) {
        int src_row  = flip ? (height - 1 - row) : row;
        size_t off   = fh.pixel_offset + (size_t)src_row * row_stride;

        if (off + row_stride > data_len) break;

        const uint8_t *row_data = data + off;
        uint32_t      *row_dst  = pixels + row * width;

        switch (bit_count) {
            case 1:
                decode_row_1bpp(row_data, row_dst, width, palette);
                break;
            case 4:
                decode_row_4bpp(row_data, row_dst, width, palette);
                break;
            case 8:
                decode_row_8bpp(row_data, row_dst, width, palette);
                break;
            case 16:
                decode_row_16bpp(row_data, row_dst, width,
                                 dib.red_mask, dib.green_mask,
                                 dib.blue_mask, dib.alpha_mask);
                break;
            case 24:
                decode_row_24bpp(row_data, row_dst, width);
                break;
            case 32:
                decode_row_32bpp(row_data, row_dst, width,
                                 dib.red_mask, dib.green_mask,
                                 dib.blue_mask, dib.alpha_mask);
                break;
        }
    }

    free(palette);

done:
    BmpImage *img = xmalloc(sizeof(BmpImage));
    if (!img) { free(pixels); return NULL; }

    img->width    = width;
    img->height   = height;
    img->pixels   = pixels;
    img->bit_depth = bit_count;

    return img;
}

BmpImage *bmp_decode_file(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 64 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }

    uint8_t *data = xmalloc((size_t)file_size);
    if (!data) { fclose(f); return NULL; }

    size_t n = fread(data, 1, (size_t)file_size, f);
    fclose(f);

    if ((long)n != file_size) { free(data); return NULL; }

    BmpImage *img = bmp_decode(data, (size_t)file_size);
    free(data);
    return img;
}

void bmp_free(BmpImage *img) {
    if (!img) return;
    free(img->pixels);
    free(img);
}

uint32_t bmp_get_pixel(const BmpImage *img, int x, int y) {
    if (!img || x < 0 || y < 0 || x >= img->width || y >= img->height)
        return 0;
    return img->pixels[y * img->width + x];
}