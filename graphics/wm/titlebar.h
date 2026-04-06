#ifndef TITLEBAR_H
#define TITLEBAR_H

#include "../draw/draw.h"
#include <stdint.h>

#define BTN_CLOSE_COLOR    0xE81123FF
#define BTN_MAX_COLOR      0x16C60CFF
#define BTN_MIN_COLOR      0xF7B900FF
#define BTN_HOVER_DIM      0xCC000000
#define BTN_RADIUS         7
#define BTN_MARGIN         6
#define BTN_Y_OFFSET       7

void titlebar_draw(NvSurface *surface, int win_w, int win_h,
                   const char *title, int focused,
                   int maximized, int flags);

void titlebar_get_close_rect(int win_w, int win_h, int *x, int *y,
                               int *w, int *h);
void titlebar_get_max_rect(int win_w, int win_h, int *x, int *y,
                             int *w, int *h);
void titlebar_get_min_rect(int win_w, int win_h, int *x, int *y,
                             int *w, int *h);

int  titlebar_hit_close(int win_w, int win_h, int x, int y);
int  titlebar_hit_maximize(int win_w, int win_h, int x, int y);
int  titlebar_hit_minimize(int win_w, int win_h, int x, int y);

#endif