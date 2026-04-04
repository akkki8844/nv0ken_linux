#ifndef DECODE_PPM_H
#define DECODE_PPM_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    PPM_FORMAT_PBM,
    PPM_FORMAT_PGM,
    PPM_FORMAT_PPM,
    PPM_FORMAT_PAM,
} PpmFormat;

typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
    PpmFormat format;
} PpmImage;

PpmImage   *ppm_decode(const uint8_t *data, size_t data_len);
PpmImage   *ppm_decode_file(const char *path);
void        ppm_free(PpmImage *img);
uint32_t    ppm_get_pixel(const PpmImage *img, int x, int y);
const char *ppm_format_name(PpmFormat fmt);

#endif