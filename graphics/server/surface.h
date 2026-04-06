#ifndef SURFACE_H
#define SURFACE_H

#include "../draw/draw.h"
#include <stdint.h>

#define SURFACE_FLAG_OPAQUE     0x01
#define SURFACE_FLAG_ALPHA      0x02
#define SURFACE_FLAG_OFFSCREEN  0x04
#define SURFACE_FLAG_SHARED_MEM 0x08

typedef enum {
    SURF_FORMAT_ARGB32,
    SURF_FORMAT_XRGB32,
    SURF_FORMAT_RGB24,
} SurfaceFormat;

typedef struct {
    int           id;
    int           width;
    int           height;
    int           pitch;
    SurfaceFormat format;
    int           flags;
    uint32_t     *pixels;
    int           dirty;
    int           owns_pixels;
    int           shm_id;
} ServerSurface;

ServerSurface *server_surface_new(int w, int h, SurfaceFormat fmt, int flags);
ServerSurface *server_surface_from_shm(int shm_id, int w, int h,
                                        SurfaceFormat fmt);
void           server_surface_free(ServerSurface *s);

void  server_surface_clear(ServerSurface *s, uint32_t color);
void  server_surface_mark_dirty(ServerSurface *s);
void  server_surface_blit_to(const ServerSurface *s, NvSurface *dst,
                              int dx, int dy, int sx, int sy, int w, int h);
void  server_surface_blit_alpha_to(const ServerSurface *s, NvSurface *dst,
                                    int dx, int dy, int sx, int sy,
                                    int w, int h, uint8_t global_alpha);

NvSurface *server_surface_as_nv(ServerSurface *s);

#endif