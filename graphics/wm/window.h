#ifndef WINDOW_H
#define WINDOW_H

#include "../draw/draw.h"
#include <stdint.h>

void window_draw_shadow(NvSurface *surface, int x, int y, int w, int h,
                         int focused);
void window_draw_border(NvSurface *surface, int x, int y, int w, int h,
                         int focused, int border_w);
void window_draw_resize_handles(NvSurface *surface, int x, int y,
                                 int w, int h);
void window_draw_frame(NvSurface *surface, int w, int h,
                        const char *title, int focused,
                        int maximized, int flags);
void window_draw_client_bg(NvSurface *surface, int cx, int cy,
                             int cw, int ch);

#endif