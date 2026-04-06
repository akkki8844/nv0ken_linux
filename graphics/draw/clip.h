#ifndef CLIP_H
#define CLIP_H

#include "draw.h"

#define CLIP_STACK_DEPTH 16

typedef struct {
    NvRect stack[CLIP_STACK_DEPTH];
    int    depth;
    NvRect active;
    int    surface_w;
    int    surface_h;
} ClipStack;

void clip_init(ClipStack *cs, int surface_w, int surface_h);
void clip_push(ClipStack *cs, const NvRect *r);
void clip_pop(ClipStack *cs);
void clip_reset(ClipStack *cs);

const NvRect *clip_active(const ClipStack *cs);
int  clip_contains(const ClipStack *cs, int x, int y);
int  clip_rect_visible(const ClipStack *cs, const NvRect *r);

int  clip_pixel(ClipStack *cs, int *x, int *y);
int  clip_line_h(ClipStack *cs, int *x, int *y, int *w);
int  clip_line_v(ClipStack *cs, int *x, int *y, int *h);
int  clip_rect(ClipStack *cs, NvRect *r);

#endif