#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdint.h>

#define COMPOSITOR_MAX_SURFACES  64
#define COMPOSITOR_MAX_DAMAGE    32

typedef enum {
    SURFACE_NORMAL,
    SURFACE_OVERLAY,
    SURFACE_CURSOR,
    SURFACE_WALLPAPER,
    SURFACE_NOTIFICATION,
} SurfaceLayer;

typedef struct {
    int          id;
    int          x, y, w, h;
    int          z_order;
    int          visible;
    int          alpha;
    SurfaceLayer layer;
    NvSurface   *buffer;
    int          dirty;
} CompSurface;

typedef struct {
    int x, y, w, h;
} DamageRect;

typedef struct Compositor Compositor;

Compositor  *compositor_new(uint32_t *framebuffer, int fb_w, int fb_h,
                             int fb_pitch);
void         compositor_free(Compositor *c);

int          compositor_create_surface(Compositor *c, int x, int y,
                                        int w, int h, SurfaceLayer layer);
void         compositor_destroy_surface(Compositor *c, int id);
CompSurface *compositor_get_surface(Compositor *c, int id);

void         compositor_move_surface(Compositor *c, int id, int x, int y);
void         compositor_resize_surface(Compositor *c, int id, int w, int h);
void         compositor_set_alpha(Compositor *c, int id, int alpha);
void         compositor_show_surface(Compositor *c, int id);
void         compositor_hide_surface(Compositor *c, int id);
void         compositor_raise_surface(Compositor *c, int id);
void         compositor_lower_surface(Compositor *c, int id);

void         compositor_damage(Compositor *c, int x, int y, int w, int h);
void         compositor_damage_surface(Compositor *c, int id);
void         compositor_composite(Compositor *c);

NvSurface   *compositor_surface_buffer(Compositor *c, int id);
int          compositor_fb_width(const Compositor *c);
int          compositor_fb_height(const Compositor *c);

#endif