#include "cursor.h"
#include "compositor.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../draw/image.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------- */

struct CursorMgr {
    Compositor  *comp;
    int          screen_w;
    int          screen_h;

    int          x;
    int          y;
    int          prev_x;
    int          prev_y;

    CursorType   type;
    int          visible;
    int          dirty;

    int          comp_surface_id;

    CursorImage  images[CURSOR_MAX_TYPES];

    /* 16×16 saved background under the cursor for software restore */
    uint32_t     saved_bg[CURSOR_SIZE * CURSOR_SIZE];
    int          bg_saved;
    int          bg_x;
    int          bg_y;
};

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

/* -----------------------------------------------------------------------
 * Procedural fallback cursor shapes
 * Every pixel is ARGB32. Transparent = 0x00000000.
 * --------------------------------------------------------------------- */

static void build_arrow(CursorImage *img) {
    static const char *rows[CURSOR_SIZE] = {
        "X               ",
        "XX              ",
        "X.X             ",
        "X..X            ",
        "X...X           ",
        "X....X          ",
        "X.....X         ",
        "X......X        ",
        "X.......X       ",
        "X........X      ",
        "X.....XXXXX     ",
        "X..X..X         ",
        "X.X X..X        ",
        "XX  X..X        ",
        "X    X..X       ",
        "     XXXX       ",
    };
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = 0;
    img->hot_y = 0;
    for (int y = 0; y < CURSOR_SIZE; y++) {
        for (int x = 0; x < CURSOR_SIZE; x++) {
            char c = rows[y][x];
            if      (c == 'X') img->pixels[y * CURSOR_SIZE + x] = 0xFF000000;
            else if (c == '.') img->pixels[y * CURSOR_SIZE + x] = 0xFFFFFFFF;
        }
    }
    img->loaded = 1;
}

static void build_ibeam(CursorImage *img) {
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = CURSOR_SIZE / 2;
    img->hot_y = CURSOR_SIZE / 2;
    int cx = CURSOR_SIZE / 2;
    /* vertical stem */
    for (int y = 1; y < CURSOR_SIZE - 1; y++) {
        img->pixels[y * CURSOR_SIZE + cx]     = 0xFF000000;
        img->pixels[y * CURSOR_SIZE + cx - 1] = 0xFFFFFFFF;
        img->pixels[y * CURSOR_SIZE + cx + 1] = 0xFFFFFFFF;
    }
    /* top serif */
    for (int x = cx - 3; x <= cx + 3; x++) {
        img->pixels[0 * CURSOR_SIZE + x] = 0xFF000000;
        img->pixels[1 * CURSOR_SIZE + x] = 0xFF000000;
    }
    /* bottom serif */
    for (int x = cx - 3; x <= cx + 3; x++) {
        img->pixels[(CURSOR_SIZE - 2) * CURSOR_SIZE + x] = 0xFF000000;
        img->pixels[(CURSOR_SIZE - 1) * CURSOR_SIZE + x] = 0xFF000000;
    }
    img->loaded = 1;
}

static void build_resize_h(CursorImage *img) {
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = CURSOR_SIZE / 2;
    img->hot_y = CURSOR_SIZE / 2;
    int mid = CURSOR_SIZE / 2;
    /* horizontal shaft */
    for (int x = 1; x < CURSOR_SIZE - 1; x++) {
        img->pixels[mid       * CURSOR_SIZE + x] = 0xFF000000;
        img->pixels[(mid - 1) * CURSOR_SIZE + x] = 0xFFFFFFFF;
        img->pixels[(mid + 1) * CURSOR_SIZE + x] = 0xFFFFFFFF;
    }
    /* left arrowhead */
    for (int i = 0; i <= 4; i++) {
        img->pixels[(mid - i) * CURSOR_SIZE + (4 - i)] = 0xFF000000;
        img->pixels[(mid + i) * CURSOR_SIZE + (4 - i)] = 0xFF000000;
    }
    /* right arrowhead */
    for (int i = 0; i <= 4; i++) {
        img->pixels[(mid - i) * CURSOR_SIZE + (CURSOR_SIZE - 5 + i)] = 0xFF000000;
        img->pixels[(mid + i) * CURSOR_SIZE + (CURSOR_SIZE - 5 + i)] = 0xFF000000;
    }
    img->loaded = 1;
}

static void build_resize_v(CursorImage *img) {
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = CURSOR_SIZE / 2;
    img->hot_y = CURSOR_SIZE / 2;
    int mid = CURSOR_SIZE / 2;
    /* vertical shaft */
    for (int y = 1; y < CURSOR_SIZE - 1; y++) {
        img->pixels[y * CURSOR_SIZE + mid]       = 0xFF000000;
        img->pixels[y * CURSOR_SIZE + (mid - 1)] = 0xFFFFFFFF;
        img->pixels[y * CURSOR_SIZE + (mid + 1)] = 0xFFFFFFFF;
    }
    /* top arrowhead */
    for (int i = 0; i <= 4; i++) {
        img->pixels[(4 - i) * CURSOR_SIZE + (mid - i)] = 0xFF000000;
        img->pixels[(4 - i) * CURSOR_SIZE + (mid + i)] = 0xFF000000;
    }
    /* bottom arrowhead */
    for (int i = 0; i <= 4; i++) {
        img->pixels[(CURSOR_SIZE - 5 + i) * CURSOR_SIZE + (mid - i)] = 0xFF000000;
        img->pixels[(CURSOR_SIZE - 5 + i) * CURSOR_SIZE + (mid + i)] = 0xFF000000;
    }
    img->loaded = 1;
}

static void build_resize_diag(CursorImage *img, int flip) {
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = CURSOR_SIZE / 2;
    img->hot_y = CURSOR_SIZE / 2;
    for (int i = 0; i < CURSOR_SIZE; i++) {
        int x = flip ? (CURSOR_SIZE - 1 - i) : i;
        img->pixels[i * CURSOR_SIZE + x] = 0xFF000000;
        if (x > 0) img->pixels[i * CURSOR_SIZE + (x - 1)] = 0xFFFFFFFF;
        if (x < CURSOR_SIZE - 1) img->pixels[i * CURSOR_SIZE + (x + 1)] = 0xFFFFFFFF;
    }
    img->loaded = 1;
}

static void build_wait(CursorImage *img) {
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = CURSOR_SIZE / 2;
    img->hot_y = CURSOR_SIZE / 2;
    int cx = CURSOR_SIZE / 2;
    int cy = CURSOR_SIZE / 2;
    int r  = 6;
    for (int y = 0; y < CURSOR_SIZE; y++) {
        for (int x = 0; x < CURSOR_SIZE; x++) {
            int dx = x - cx, dy = y - cy;
            int d2 = dx*dx + dy*dy;
            if (d2 >= (r-1)*(r-1) && d2 <= r*r + 2)
                img->pixels[y * CURSOR_SIZE + x] = 0xFF000000;
            else if (d2 < (r-1)*(r-1) && d2 >= (r-3)*(r-3))
                img->pixels[y * CURSOR_SIZE + x] = 0xFFFFFFFF;
        }
    }
    /* center dot */
    img->pixels[ cy      * CURSOR_SIZE + cx]     = 0xFF000000;
    img->pixels[(cy - 1) * CURSOR_SIZE + cx]     = 0xFF000000;
    img->pixels[(cy + 1) * CURSOR_SIZE + cx]     = 0xFF000000;
    img->pixels[ cy      * CURSOR_SIZE + (cx-1)] = 0xFF000000;
    img->pixels[ cy      * CURSOR_SIZE + (cx+1)] = 0xFF000000;
    img->loaded = 1;
}

static void build_crosshair(CursorImage *img) {
    memset(img->pixels, 0, sizeof(img->pixels));
    img->hot_x = CURSOR_SIZE / 2;
    img->hot_y = CURSOR_SIZE / 2;
    int mid = CURSOR_SIZE / 2;
    for (int i = 0; i < CURSOR_SIZE; i++) {
        if (i == mid) continue;
        img->pixels[mid * CURSOR_SIZE + i] = 0xFF000000;
        img->pixels[i   * CURSOR_SIZE + mid] = 0xFF000000;
    }
    /* gap in centre */
    for (int d = -2; d <= 2; d++) {
        img->pixels[mid * CURSOR_SIZE + (mid + d)] = 0x00000000;
        img->pixels[(mid + d) * CURSOR_SIZE + mid] = 0x00000000;
    }
    img->loaded = 1;
}

static void build_all_defaults(CursorMgr *cm) {
    build_arrow(&cm->images[CURSOR_ARROW]);
    build_ibeam(&cm->images[CURSOR_IBEAM]);
    build_resize_h(&cm->images[CURSOR_RESIZE]);
    build_wait(&cm->images[CURSOR_WAIT]);
    /* CURSOR_HIDDEN: leave empty */
    build_resize_h(&cm->images[CURSOR_RESIZE_H]);
    build_resize_v(&cm->images[CURSOR_RESIZE_V]);
    build_crosshair(&cm->images[CURSOR_CROSSHAIR]);
}

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

CursorMgr *cursor_mgr_new(Compositor *comp, int screen_w, int screen_h) {
    CursorMgr *cm = xmalloc(sizeof(CursorMgr));
    if (!cm) return NULL;

    cm->comp        = comp;
    cm->screen_w    = screen_w;
    cm->screen_h    = screen_h;
    cm->x           = screen_w / 2;
    cm->y           = screen_h / 2;
    cm->prev_x      = cm->x;
    cm->prev_y      = cm->y;
    cm->type        = CURSOR_ARROW;
    cm->visible     = 1;
    cm->dirty       = 1;
    cm->bg_saved    = 0;
    cm->comp_surface_id = -1;

    build_all_defaults(cm);

    if (comp) {
        cm->comp_surface_id = compositor_create_surface(
            comp,
            cm->x, cm->y,
            CURSOR_SIZE, CURSOR_SIZE,
            SURFACE_CURSOR);

        if (cm->comp_surface_id > 0) {
            NvSurface *surf = compositor_surface_buffer(
                comp, cm->comp_surface_id);
            if (surf) {
                CursorImage *img = &cm->images[CURSOR_ARROW];
                for (int py = 0; py < CURSOR_SIZE; py++)
                    for (int px = 0; px < CURSOR_SIZE; px++)
                        draw_pixel(surf, px, py,
                                   img->pixels[py * CURSOR_SIZE + px]);
            }
        }
    }

    return cm;
}

void cursor_mgr_free(CursorMgr *cm) {
    if (!cm) return;
    if (cm->comp && cm->comp_surface_id > 0)
        compositor_destroy_surface(cm->comp, cm->comp_surface_id);
    free(cm);
}

/* -----------------------------------------------------------------------
 * Load cursor images from PPM files
 * --------------------------------------------------------------------- */

void cursor_mgr_load_from_dir(CursorMgr *cm, const char *dir) {
    if (!cm || !dir) return;

    static const struct { CursorType type; const char *name; } map[] = {
        { CURSOR_ARROW,     "arrow"     },
        { CURSOR_IBEAM,     "ibeam"     },
        { CURSOR_RESIZE,    "resize"    },
        { CURSOR_WAIT,      "wait"      },
        { CURSOR_RESIZE_H,  "resize_h"  },
        { CURSOR_RESIZE_V,  "resize_v"  },
        { CURSOR_CROSSHAIR, "crosshair" },
    };

    for (int i = 0; i < (int)(sizeof(map)/sizeof(map[0])); i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.ppm", dir, map[i].name);

        NvImage *img = image_load(path);
        if (!img) continue;

        CursorImage *ci = &cm->images[map[i].type];
        memset(ci->pixels, 0, sizeof(ci->pixels));

        int copy_w = img->width  < CURSOR_SIZE ? img->width  : CURSOR_SIZE;
        int copy_h = img->height < CURSOR_SIZE ? img->height : CURSOR_SIZE;

        for (int py = 0; py < copy_h; py++)
            for (int px = 0; px < copy_w; px++)
                ci->pixels[py * CURSOR_SIZE + px] =
                    img->pixels[py * img->width + px];

        /* hotspot convention: pixel at (0,0) is magenta = hotspot marker */
        if (img->pixels[0] == 0xFFFF00FF) {
            ci->hot_x = 0;
            ci->hot_y = 0;
            ci->pixels[0] = 0x00000000;
        } else {
            ci->hot_x = 0;
            ci->hot_y = 0;
        }

        ci->loaded = 1;
        image_free(img);
    }

    cursor_mgr_invalidate(cm);
}

/* -----------------------------------------------------------------------
 * Type and visibility
 * --------------------------------------------------------------------- */

void cursor_mgr_set_type(CursorMgr *cm, CursorType type) {
    if (!cm || cm->type == type) return;
    cm->type  = type;
    cm->dirty = 1;

    if (cm->comp && cm->comp_surface_id > 0) {
        if (type == CURSOR_HIDDEN || !cm->images[type].loaded) {
            compositor_hide_surface(cm->comp, cm->comp_surface_id);
        } else {
            NvSurface *surf = compositor_surface_buffer(
                cm->comp, cm->comp_surface_id);
            if (surf) {
                surface_clear(surf, 0x00000000);
                CursorImage *ci = &cm->images[type];
                for (int py = 0; py < CURSOR_SIZE; py++)
                    for (int px = 0; px < CURSOR_SIZE; px++)
                        draw_pixel(surf, px, py,
                                   ci->pixels[py * CURSOR_SIZE + px]);
            }
            compositor_show_surface(cm->comp, cm->comp_surface_id);
            compositor_damage_surface(cm->comp, cm->comp_surface_id);
        }
    }
}

CursorType cursor_mgr_type(const CursorMgr *cm) {
    return cm ? cm->type : CURSOR_ARROW;
}

void cursor_mgr_show(CursorMgr *cm) {
    if (!cm || cm->visible) return;
    cm->visible = 1;
    cm->dirty   = 1;
    if (cm->comp && cm->comp_surface_id > 0 && cm->type != CURSOR_HIDDEN)
        compositor_show_surface(cm->comp, cm->comp_surface_id);
}

void cursor_mgr_hide(CursorMgr *cm) {
    if (!cm || !cm->visible) return;
    cm->visible = 0;
    cm->dirty   = 1;
    if (cm->comp && cm->comp_surface_id > 0)
        compositor_hide_surface(cm->comp, cm->comp_surface_id);
}

int cursor_mgr_visible(const CursorMgr *cm) {
    return cm ? cm->visible : 0;
}

/* -----------------------------------------------------------------------
 * Movement
 * --------------------------------------------------------------------- */

void cursor_mgr_move(CursorMgr *cm, int x, int y) {
    if (!cm) return;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= cm->screen_w) x = cm->screen_w - 1;
    if (y >= cm->screen_h) y = cm->screen_h - 1;

    if (cm->x == x && cm->y == y) return;

    cm->prev_x = cm->x;
    cm->prev_y = cm->y;
    cm->x      = x;
    cm->y      = y;
    cm->dirty  = 1;
    cm->bg_saved = 0;

    if (cm->comp && cm->comp_surface_id > 0) {
        CursorImage *ci = &cm->images[cm->type];
        compositor_move_surface(cm->comp, cm->comp_surface_id,
                                x - ci->hot_x, y - ci->hot_y);
    }
}

int cursor_mgr_x(const CursorMgr *cm) { return cm ? cm->x : 0; }
int cursor_mgr_y(const CursorMgr *cm) { return cm ? cm->y : 0; }

void cursor_mgr_invalidate(CursorMgr *cm) {
    if (cm) { cm->dirty = 1; cm->bg_saved = 0; }
}

/* -----------------------------------------------------------------------
 * Software background save/restore — used when compositing is
 * unavailable or for direct-to-framebuffer rendering without a
 * compositor surface layer.
 * --------------------------------------------------------------------- */

void cursor_mgr_save_bg(CursorMgr *cm, NvSurface *surface) {
    if (!cm || !surface) return;

    CursorImage *ci = &cm->images[cm->type];
    int ox = cm->x - ci->hot_x;
    int oy = cm->y - ci->hot_y;

    for (int py = 0; py < CURSOR_SIZE; py++) {
        for (int px = 0; px < CURSOR_SIZE; px++) {
            int sx = ox + px;
            int sy = oy + py;
            if (sx < 0 || sy < 0 ||
                sx >= surface->width || sy >= surface->height) {
                cm->saved_bg[py * CURSOR_SIZE + px] = 0x00000000;
            } else {
                cm->saved_bg[py * CURSOR_SIZE + px] =
                    surface->pixels[sy * surface->pitch + sx];
            }
        }
    }

    cm->bg_saved = 1;
    cm->bg_x     = ox;
    cm->bg_y     = oy;
}

void cursor_mgr_restore_bg(CursorMgr *cm, NvSurface *surface) {
    if (!cm || !surface || !cm->bg_saved) return;

    for (int py = 0; py < CURSOR_SIZE; py++) {
        for (int px = 0; px < CURSOR_SIZE; px++) {
            int sx = cm->bg_x + px;
            int sy = cm->bg_y + py;
            if (sx >= 0 && sy >= 0 &&
                sx < surface->width && sy < surface->height) {
                surface->pixels[sy * surface->pitch + sx] =
                    cm->saved_bg[py * CURSOR_SIZE + px];
            }
        }
    }

    cm->bg_saved = 0;
}

/* -----------------------------------------------------------------------
 * Software draw — blits cursor pixels onto the surface with alpha
 * --------------------------------------------------------------------- */

void cursor_mgr_draw(CursorMgr *cm, NvSurface *surface) {
    if (!cm || !surface || !cm->visible) return;
    if (cm->type == CURSOR_HIDDEN) return;
    if (!cm->images[cm->type].loaded) return;

    CursorImage *ci = &cm->images[cm->type];
    int ox = cm->x - ci->hot_x;
    int oy = cm->y - ci->hot_y;

    for (int py = 0; py < CURSOR_SIZE; py++) {
        for (int px = 0; px < CURSOR_SIZE; px++) {
            uint32_t c = ci->pixels[py * CURSOR_SIZE + px];
            if (COLOR_A(c) == 0) continue;
            int sx = ox + px;
            int sy = oy + py;
            if (sx < 0 || sy < 0 ||
                sx >= surface->width || sy >= surface->height) continue;
            uint32_t *dst = &surface->pixels[sy * surface->pitch + sx];
            *dst = color_blend(*dst, c);
        }
    }

    cm->dirty = 0;
}