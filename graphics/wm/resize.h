#ifndef RESIZE_H
#define RESIZE_H

#include "wm.h"
#include "../draw/draw.h"

typedef struct {
    int     win_id;
    HitZone edge;
    int     active;
    int     start_mx, start_my;
    int     start_wx, start_wy;
    int     start_ww, start_wh;
    int     min_w, min_h;
    int     max_w, max_h;
    int     grid_snap;
} ResizeState;

void resize_begin(ResizeState *rs, int win_id, HitZone edge,
                  int mx, int my, int wx, int wy,
                  int ww, int wh, int min_w, int min_h);
void resize_update(ResizeState *rs, int mx, int my,
                   int *out_x, int *out_y, int *out_w, int *out_h);
void resize_end(ResizeState *rs);
int  resize_active(const ResizeState *rs);
void resize_set_constraints(ResizeState *rs, int min_w, int min_h,
                             int max_w, int max_h);
void resize_set_grid(ResizeState *rs, int grid);

#endif