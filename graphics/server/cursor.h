#ifndef CURSOR_H
#define CURSOR_H

#include "compositor.h"
#include "../draw/draw.h"
#include "../draw/image.h"
#include <stdint.h>

#define CURSOR_SIZE       16
#define CURSOR_MAX_TYPES  8

/* -----------------------------------------------------------------------
 * Cursor type constants — match IpcSetCursor values in ipc_server.h
 * --------------------------------------------------------------------- */

typedef enum {
    CURSOR_ARROW    = 0,
    CURSOR_IBEAM    = 1,
    CURSOR_RESIZE   = 2,
    CURSOR_WAIT     = 3,
    CURSOR_HIDDEN   = 4,
    CURSOR_RESIZE_H = 5,
    CURSOR_RESIZE_V = 6,
    CURSOR_CROSSHAIR= 7,
} CursorType;

/* -----------------------------------------------------------------------
 * A single cursor image — 16×16 ARGB32 pixel grid with hotspot
 * --------------------------------------------------------------------- */

typedef struct {
    uint32_t pixels[CURSOR_SIZE * CURSOR_SIZE];
    int      hot_x;
    int      hot_y;
    int      loaded;
} CursorImage;

/* -----------------------------------------------------------------------
 * Manager
 * --------------------------------------------------------------------- */

typedef struct CursorMgr CursorMgr;

CursorMgr *cursor_mgr_new(Compositor *comp, int screen_w, int screen_h);
void       cursor_mgr_free(CursorMgr *cm);

void       cursor_mgr_load_from_dir(CursorMgr *cm, const char *dir);
void       cursor_mgr_set_type(CursorMgr *cm, CursorType type);
CursorType cursor_mgr_type(const CursorMgr *cm);

void       cursor_mgr_move(CursorMgr *cm, int x, int y);
int        cursor_mgr_x(const CursorMgr *cm);
int        cursor_mgr_y(const CursorMgr *cm);

void       cursor_mgr_show(CursorMgr *cm);
void       cursor_mgr_hide(CursorMgr *cm);
int        cursor_mgr_visible(const CursorMgr *cm);

void       cursor_mgr_draw(CursorMgr *cm, NvSurface *surface);
void       cursor_mgr_invalidate(CursorMgr *cm);

void       cursor_mgr_save_bg(CursorMgr *cm, NvSurface *surface);
void       cursor_mgr_restore_bg(CursorMgr *cm, NvSurface *surface);

#endif