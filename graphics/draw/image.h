#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
} NvImage;

NvImage *image_load(const char *path);
NvImage *image_load_bmp(const char *path);
NvImage *image_load_ppm(const char *path);
NvImage *image_from_pixels(uint32_t *pixels, int w, int h);
void     image_free(NvImage *img);

NvImage *image_scale(const NvImage *src, int new_w, int new_h);
NvImage *image_crop(const NvImage *src, int x, int y, int w, int h);
void     image_flip_h(NvImage *img);
void     image_flip_v(NvImage *img);

#endif