#ifndef DISPLAY_H
#define DISPLAY_H

#include "../draw/draw.h"
#include "compositor.h"
#include <stdint.h>

typedef struct {
    int      width;
    int      height;
    int      pitch;
    int      bpp;
    uint32_t phys_addr;
    uint32_t *pixels;
} DisplayInfo;

typedef struct Display Display;

Display    *display_new(uint32_t *fb_addr, int w, int h,
                         int pitch, int bpp);
void        display_free(Display *d);

int         display_width(const Display *d);
int         display_height(const Display *d);
int         display_pitch(const Display *d);
uint32_t   *display_pixels(const Display *d);
Compositor *display_compositor(const Display *d);

void        display_vsync(Display *d);
void        display_present(Display *d);
void        display_clear(Display *d, uint32_t color);
void        display_get_info(const Display *d, DisplayInfo *info);

#endif