#ifndef LAYOUT_H
#define LAYOUT_H

#include "wm.h"

typedef enum {
    LAYOUT_FLOATING,
    LAYOUT_TILE_H,
    LAYOUT_TILE_V,
    LAYOUT_MONOCLE,
} LayoutMode;

void layout_apply(WindowManager *wm, LayoutMode mode,
                  int area_x, int area_y, int area_w, int area_h);
void layout_tile_h(WindowManager *wm,
                   int ax, int ay, int aw, int ah);
void layout_tile_v(WindowManager *wm,
                   int ax, int ay, int aw, int ah);
void layout_monocle(WindowManager *wm,
                    int ax, int ay, int aw, int ah);

#endif