#ifndef DECODE_BMP_H
#define DECODE_BMP_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
    int       bit_depth;
} BmpImage;

BmpImage *bmp_decode(const uint8_t *data, size_t data_len);
BmpImage *bmp_decode_file(const char *path);
void      bmp_free(BmpImage *img);
uint32_t  bmp_get_pixel(const BmpImage *img, int x, int y);

#endif