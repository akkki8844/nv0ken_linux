#include "display.h"
#include "compositor.h"
#include "../draw/draw.h"
#include <stdlib.h>
#include <string.h>

struct Display {
    uint32_t   *pixels;
    int         width;
    int         height;
    int         pitch;
    int         bpp;
    Compositor *comp;
    NvSurface  *fb_surface;
};

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

Display *display_new(uint32_t *fb_addr, int w, int h, int pitch, int bpp) {
    if (!fb_addr || w <= 0 || h <= 0) return NULL;

    Display *d = xmalloc(sizeof(Display));
    if (!d) return NULL;

    d->pixels     = fb_addr;
    d->width      = w;
    d->height     = h;
    d->pitch      = pitch > 0 ? pitch : w;
    d->bpp        = bpp;
    d->fb_surface = surface_from_pixels(fb_addr, w, h, d->pitch);
    d->comp       = compositor_new(fb_addr, w, h, d->pitch);

    if (!d->fb_surface || !d->comp) {
        surface_free(d->fb_surface);
        compositor_free(d->comp);
        free(d);
        return NULL;
    }

    return d;
}

void display_free(Display *d) {
    if (!d) return;
    compositor_free(d->comp);
    surface_free(d->fb_surface);
    free(d);
}

int      display_width(const Display *d)  { return d ? d->width  : 0; }
int      display_height(const Display *d) { return d ? d->height : 0; }
int      display_pitch(const Display *d)  { return d ? d->pitch  : 0; }
uint32_t *display_pixels(const Display *d){ return d ? d->pixels : NULL; }
Compositor *display_compositor(const Display *d) { return d ? d->comp : NULL; }

void display_vsync(Display *d) {
    if (!d) return;
    /*
     * On real hardware this would wait for the VBlank interrupt from the
     * display controller (e.g. read from a VBlank-wait ioctl or spin on
     * a CRTC status register bit). In QEMU there is no VBlank, so this
     * is a no-op that can be replaced with a real implementation once the
     * display driver exists.
     */
}

void display_present(Display *d) {
    if (!d || !d->comp) return;
    compositor_composite(d->comp);
}

void display_clear(Display *d, uint32_t color) {
    if (!d || !d->pixels) return;
    int total = d->pitch * d->height;
    for (int i = 0; i < total; i++) d->pixels[i] = color;
}

void display_get_info(const Display *d, DisplayInfo *info) {
    if (!d || !info) return;
    info->width     = d->width;
    info->height    = d->height;
    info->pitch     = d->pitch;
    info->bpp       = d->bpp;
    info->phys_addr = 0;
    info->pixels    = d->pixels;
}