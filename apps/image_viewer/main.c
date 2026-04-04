#include "decode_bmp.h"
#include "decode_ppm.h"
#include "../../../libnv0/include/nv0/app.h"
#include "../../../libnv0/include/nv0/window.h"
#include "../../../libnv0/include/nv0/input.h"
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../libnv0/include/nv0/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define WINDOW_W        900
#define WINDOW_H        660
#define TOOLBAR_H       40
#define STATUSBAR_H     24
#define CANVAS_Y        TOOLBAR_H
#define CANVAS_W        WINDOW_W
#define CANVAS_H        (WINDOW_H - TOOLBAR_H - STATUSBAR_H)
#define ZOOM_MIN        0.05f
#define ZOOM_MAX        32.0f
#define ZOOM_STEP       1.25f
#define MAX_PATH_LEN    1024
#define MAX_RECENT      16

#define COL_BG          NV_COLOR(0x28, 0x28, 0x28, 0xFF)
#define COL_CANVAS_BG   NV_COLOR(0x1A, 0x1A, 0x1A, 0xFF)
#define COL_TOOLBAR_BG  NV_COLOR(0x38, 0x38, 0x38, 0xFF)
#define COL_STATUS_BG   NV_COLOR(0x38, 0x38, 0x38, 0xFF)
#define COL_BTN_BG      NV_COLOR(0x50, 0x50, 0x50, 0xFF)
#define COL_BTN_FG      NV_COLOR(0xEE, 0xEE, 0xEE, 0xFF)
#define COL_BORDER      NV_COLOR(0x22, 0x22, 0x22, 0xFF)
#define COL_TEXT        NV_COLOR(0xEE, 0xEE, 0xEE, 0xFF)
#define COL_TEXT_MUTED  NV_COLOR(0x88, 0x88, 0x88, 0xFF)
#define COL_CHECKER_A   NV_COLOR(0x60, 0x60, 0x60, 0xFF)
#define COL_CHECKER_B   NV_COLOR(0x50, 0x50, 0x50, 0xFF)
#define CHECKER_SIZE    12

typedef enum {
    FMT_UNKNOWN,
    FMT_PPM,
    FMT_PGM,
    FMT_PBM,
    FMT_PAM,
    FMT_BMP,
} ImageFormat;

typedef struct {
    uint32_t *pixels;
    int       width;
    int       height;
    ImageFormat format;
    char      path[MAX_PATH_LEN];
} Image;

typedef struct {
    NvWindow  *window;
    NvSurface *surface;

    Image     *img;

    float      zoom;
    int        pan_x;
    int        pan_y;
    int        drag_active;
    int        drag_start_x;
    int        drag_start_y;
    int        drag_pan_x;
    int        drag_pan_y;

    int        fit_mode;

    char       status[256];
    char       recent[MAX_RECENT][MAX_PATH_LEN];
    int        recent_count;
} Viewer;

static Viewer g_viewer;

/* -----------------------------------------------------------------------
 * Image load/free
 * --------------------------------------------------------------------- */

static ImageFormat detect_format(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return FMT_UNKNOWN;
    ext++;
    if (strcasecmp(ext, "bmp") == 0) return FMT_BMP;
    if (strcasecmp(ext, "ppm") == 0) return FMT_PPM;
    if (strcasecmp(ext, "pgm") == 0) return FMT_PGM;
    if (strcasecmp(ext, "pbm") == 0) return FMT_PBM;
    if (strcasecmp(ext, "pam") == 0) return FMT_PAM;
    return FMT_UNKNOWN;
}

static const char *format_name(ImageFormat fmt) {
    switch (fmt) {
        case FMT_BMP: return "BMP";
        case FMT_PPM: return "PPM";
        case FMT_PGM: return "PGM";
        case FMT_PBM: return "PBM";
        case FMT_PAM: return "PAM";
        default:      return "Unknown";
    }
}

static void image_free(Image *img) {
    if (!img) return;
    free(img->pixels);
    free(img);
}

static Image *image_load(const char *path) {
    if (!path) return NULL;

    ImageFormat fmt = detect_format(path);
    uint32_t *pixels = NULL;
    int width = 0, height = 0;

    if (fmt == FMT_BMP) {
        BmpImage *bmp = bmp_decode_file(path);
        if (!bmp) return NULL;
        pixels = bmp->pixels;
        width  = bmp->width;
        height = bmp->height;
        bmp->pixels = NULL;
        bmp_free(bmp);
    } else if (fmt == FMT_PPM || fmt == FMT_PGM ||
               fmt == FMT_PBM || fmt == FMT_PAM ||
               fmt == FMT_UNKNOWN) {
        PpmImage *ppm = ppm_decode_file(path);
        if (!ppm) return NULL;
        pixels = ppm->pixels;
        width  = ppm->width;
        height = ppm->height;
        fmt = fmt == FMT_UNKNOWN ? FMT_PPM : fmt;
        ppm->pixels = NULL;
        ppm_free(ppm);
    } else {
        return NULL;
    }

    if (!pixels) return NULL;

    Image *img = malloc(sizeof(Image));
    if (!img) { free(pixels); return NULL; }

    img->pixels = pixels;
    img->width  = width;
    img->height = height;
    img->format = fmt;
    strncpy(img->path, path, MAX_PATH_LEN - 1);
    img->path[MAX_PATH_LEN - 1] = '\0';

    return img;
}

/* -----------------------------------------------------------------------
 * Zoom and pan helpers
 * --------------------------------------------------------------------- */

static void viewer_fit(Viewer *v) {
    if (!v->img) return;

    float scale_x = (float)CANVAS_W / (float)v->img->width;
    float scale_y = (float)CANVAS_H / (float)v->img->height;
    v->zoom  = scale_x < scale_y ? scale_x : scale_y;
    if (v->zoom > 1.0f) v->zoom = 1.0f;

    v->pan_x = (CANVAS_W - (int)(v->img->width  * v->zoom)) / 2;
    v->pan_y = (CANVAS_Y + CANVAS_H / 2) - (int)(v->img->height * v->zoom) / 2;
}

static void viewer_zoom_at(Viewer *v, float new_zoom, int cx, int cy) {
    if (new_zoom < ZOOM_MIN) new_zoom = ZOOM_MIN;
    if (new_zoom > ZOOM_MAX) new_zoom = ZOOM_MAX;

    float ratio = new_zoom / v->zoom;
    v->pan_x = (int)(cx - ratio * (cx - v->pan_x));
    v->pan_y = (int)(cy - ratio * (cy - v->pan_y));
    v->zoom  = new_zoom;
}

static void viewer_zoom_in(Viewer *v) {
    int cx = CANVAS_W / 2;
    int cy = CANVAS_Y + CANVAS_H / 2;
    viewer_zoom_at(v, v->zoom * ZOOM_STEP, cx, cy);
}

static void viewer_zoom_out(Viewer *v) {
    int cx = CANVAS_W / 2;
    int cy = CANVAS_Y + CANVAS_H / 2;
    viewer_zoom_at(v, v->zoom / ZOOM_STEP, cx, cy);
}

static void viewer_zoom_1to1(Viewer *v) {
    v->zoom  = 1.0f;
    v->pan_x = (CANVAS_W - v->img->width)  / 2;
    v->pan_y = CANVAS_Y + (CANVAS_H - v->img->height) / 2;
}

/* -----------------------------------------------------------------------
 * Open image
 * --------------------------------------------------------------------- */

static void viewer_open(Viewer *v, const char *path) {
    Image *img = image_load(path);
    if (!img) {
        char msg[MAX_PATH_LEN + 32];
        snprintf(msg, sizeof(msg), "Failed to open: %s", path);
        strncpy(v->status, msg, sizeof(v->status) - 1);
        nv_surface_invalidate(v->surface);
        return;
    }

    image_free(v->img);
    v->img = img;
    v->fit_mode = 1;
    viewer_fit(v);

    if (v->recent_count < MAX_RECENT) {
        strncpy(v->recent[v->recent_count++], path, MAX_PATH_LEN - 1);
    } else {
        memmove(&v->recent[0], &v->recent[1],
                sizeof(v->recent[0]) * (MAX_RECENT - 1));
        strncpy(v->recent[MAX_RECENT - 1], path, MAX_PATH_LEN - 1);
    }

    char title[MAX_PATH_LEN + 32];
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    snprintf(title, sizeof(title), "Image Viewer — %s", name);
    nv_window_set_title(v->window, title);

    snprintf(v->status, sizeof(v->status),
             "%s  %d x %d  %s  %.0f%%",
             name, img->width, img->height,
             format_name(img->format), v->zoom * 100.0f);

    nv_surface_invalidate(v->surface);
}

static void viewer_open_dialog(Viewer *v) {
    char path[MAX_PATH_LEN] = {0};
    if (nv_dialog_open_file("Open Image", path, sizeof(path),
                             "*.bmp;*.ppm;*.pgm;*.pbm;*.pam") != 1) return;
    if (path[0]) viewer_open(v, path);
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_checker(NvSurface *surface, int x, int y, int w, int h) {
    for (int py = y; py < y + h; py += CHECKER_SIZE) {
        for (int px = x; px < x + w; px += CHECKER_SIZE) {
            int even = ((px / CHECKER_SIZE) + (py / CHECKER_SIZE)) % 2 == 0;
            int rw = px + CHECKER_SIZE > x + w ? x + w - px : CHECKER_SIZE;
            int rh = py + CHECKER_SIZE > y + h ? y + h - py : CHECKER_SIZE;
            NvRect r = { px, py, rw, rh };
            nv_draw_fill_rect(surface, &r, even ? COL_CHECKER_A : COL_CHECKER_B);
        }
    }
}

static void draw_image(Viewer *v) {
    if (!v->img) return;

    int img_w = (int)(v->img->width  * v->zoom);
    int img_h = (int)(v->img->height * v->zoom);
    int img_x = v->pan_x;
    int img_y = v->pan_y;

    int clip_x = img_x < 0 ? 0 : img_x;
    int clip_y = img_y < CANVAS_Y ? CANVAS_Y : img_y;
    int clip_x2 = img_x + img_w > CANVAS_W ? CANVAS_W : img_x + img_w;
    int clip_y2 = img_y + img_h > CANVAS_Y + CANVAS_H
                  ? CANVAS_Y + CANVAS_H : img_y + img_h;

    if (clip_x >= clip_x2 || clip_y >= clip_y2) return;

    draw_checker(v->surface, clip_x, clip_y,
                 clip_x2 - clip_x, clip_y2 - clip_y);

    for (int py = clip_y; py < clip_y2; py++) {
        int src_y = (int)((py - img_y) / v->zoom);
        if (src_y < 0 || src_y >= v->img->height) continue;

        for (int px = clip_x; px < clip_x2; px++) {
            int src_x = (int)((px - img_x) / v->zoom);
            if (src_x < 0 || src_x >= v->img->width) continue;

            uint32_t argb = v->img->pixels[src_y * v->img->width + src_x];
            uint8_t a = (argb >> 24) & 0xFF;
            uint8_t r = (argb >> 16) & 0xFF;
            uint8_t g = (argb >>  8) & 0xFF;
            uint8_t b =  argb        & 0xFF;

            if (a == 0xFF) {
                nv_draw_pixel(v->surface, px, py, NV_COLOR(r, g, b, 0xFF));
            } else if (a > 0) {
                int ci = ((px / CHECKER_SIZE) + (py / CHECKER_SIZE)) % 2;
                uint8_t bg = ci ? 0x50 : 0x60;
                uint8_t ro = (uint8_t)((r * a + bg * (255 - a)) / 255);
                uint8_t go = (uint8_t)((g * a + bg * (255 - a)) / 255);
                uint8_t bo = (uint8_t)((b * a + bg * (255 - a)) / 255);
                nv_draw_pixel(v->surface, px, py, NV_COLOR(ro, go, bo, 0xFF));
            }
        }
    }
}

static void draw_toolbar(Viewer *v) {
    NvRect bg = {0, 0, WINDOW_W, TOOLBAR_H};
    nv_draw_fill_rect(v->surface, &bg, COL_TOOLBAR_BG);
    NvRect border = {0, TOOLBAR_H - 1, WINDOW_W, 1};
    nv_draw_fill_rect(v->surface, &border, COL_BORDER);

    struct { int x; const char *label; int enabled; } btns[] = {
        {  4, "Open",    1                  },
        { 60, "Fit",     v->img != NULL     },
        {100, "1:1",     v->img != NULL     },
        {136, "+",       v->img != NULL     },
        {160, "-",       v->img != NULL     },
        {184, "Rotate",  v->img != NULL     },
        {248, "Info",    v->img != NULL     },
        { -1, NULL,      0                  },
    };

    for (int i = 0; btns[i].label; i++) {
        int bw = (int)strlen(btns[i].label) * 8 + 16;
        NvRect r = { btns[i].x, 6, bw, 28 };
        NvColor bg_c = btns[i].enabled
                       ? COL_BTN_BG
                       : NV_COLOR(0x42, 0x42, 0x42, 0xFF);
        NvColor fg_c = btns[i].enabled ? COL_BTN_FG : COL_TEXT_MUTED;
        nv_draw_fill_rect(v->surface, &r, bg_c);
        nv_draw_rect(v->surface, &r, COL_BORDER);
        nv_draw_text(v->surface, btns[i].x + 8, 13, btns[i].label, fg_c);
    }

    if (v->img) {
        char zoom_str[32];
        snprintf(zoom_str, sizeof(zoom_str), "%.0f%%", v->zoom * 100.0f);
        nv_draw_text(v->surface, WINDOW_W - 60, 13, zoom_str, COL_TEXT_MUTED);
    }
}

static void draw_statusbar(Viewer *v) {
    int y = WINDOW_H - STATUSBAR_H;
    NvRect bg = {0, y, WINDOW_W, STATUSBAR_H};
    nv_draw_fill_rect(v->surface, &bg, COL_STATUS_BG);
    NvRect border = {0, y, WINDOW_W, 1};
    nv_draw_fill_rect(v->surface, &border, COL_BORDER);
    nv_draw_text(v->surface, 8, y + 5, v->status, COL_TEXT_MUTED);
}

static void on_paint(NvWindow *win, NvSurface *surface, void *userdata) {
    (void)win;
    Viewer *v = (Viewer *)userdata;
    v->surface = surface;

    NvRect canvas = {0, CANVAS_Y, CANVAS_W, CANVAS_H};
    nv_draw_fill_rect(surface, &canvas, COL_CANVAS_BG);

    draw_toolbar(v);

    if (!v->img) {
        const char *hint = "Open an image to get started";
        int hw = (int)strlen(hint) * 8;
        nv_draw_text(surface,
                     CANVAS_W / 2 - hw / 2,
                     CANVAS_Y + CANVAS_H / 2,
                     hint, COL_TEXT_MUTED);
    } else {
        draw_image(v);
    }

    draw_statusbar(v);
}

/* -----------------------------------------------------------------------
 * Toolbar hit test
 * --------------------------------------------------------------------- */

typedef enum {
    TB_NONE,
    TB_OPEN,
    TB_FIT,
    TB_1TO1,
    TB_ZOOM_IN,
    TB_ZOOM_OUT,
    TB_ROTATE,
    TB_INFO,
} ToolbarBtn;

static ToolbarBtn toolbar_hit(int x, int y) {
    if (y < 6 || y > 34) return TB_NONE;
    if (x >=   4 && x <  52) return TB_OPEN;
    if (x >=  60 && x <  96) return TB_FIT;
    if (x >= 100 && x < 132) return TB_1TO1;
    if (x >= 136 && x < 156) return TB_ZOOM_IN;
    if (x >= 160 && x < 180) return TB_ZOOM_OUT;
    if (x >= 184 && x < 244) return TB_ROTATE;
    if (x >= 248 && x < 288) return TB_INFO;
    return TB_NONE;
}

/* -----------------------------------------------------------------------
 * Rotate
 * --------------------------------------------------------------------- */

static void viewer_rotate_cw(Viewer *v) {
    if (!v->img) return;
    int w = v->img->width;
    int h = v->img->height;
    uint32_t *dst = malloc(sizeof(uint32_t) * w * h);
    if (!dst) return;

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            dst[x * h + (h - 1 - y)] = v->img->pixels[y * w + x];

    free(v->img->pixels);
    v->img->pixels = dst;
    v->img->width  = h;
    v->img->height = w;

    if (v->fit_mode) viewer_fit(v);

    snprintf(v->status, sizeof(v->status),
             "%d x %d  %s  %.0f%%",
             v->img->width, v->img->height,
             format_name(v->img->format), v->zoom * 100.0f);

    nv_surface_invalidate(v->surface);
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

static void on_mouse_down(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    Viewer *v = (Viewer *)userdata;

    if (ev->y < TOOLBAR_H) {
        ToolbarBtn btn = toolbar_hit(ev->x, ev->y);
        switch (btn) {
            case TB_OPEN:    viewer_open_dialog(v);  break;
            case TB_FIT:     v->fit_mode = 1; viewer_fit(v); break;
            case TB_1TO1:    v->fit_mode = 0; viewer_zoom_1to1(v); break;
            case TB_ZOOM_IN: v->fit_mode = 0; viewer_zoom_in(v);   break;
            case TB_ZOOM_OUT:v->fit_mode = 0; viewer_zoom_out(v);  break;
            case TB_ROTATE:  viewer_rotate_cw(v);    break;
            case TB_INFO: {
                if (!v->img) break;
                char info[512];
                snprintf(info, sizeof(info),
                         "File: %s\nFormat: %s\nDimensions: %d x %d\nZoom: %.0f%%",
                         v->img->path,
                         format_name(v->img->format),
                         v->img->width, v->img->height,
                         v->zoom * 100.0f);
                nv_dialog_info("Image Info", info);
                break;
            }
            default: break;
        }
        nv_surface_invalidate(v->surface);
        return;
    }

    if (ev->button == NV_MOUSE_LEFT) {
        v->drag_active  = 1;
        v->drag_start_x = ev->x;
        v->drag_start_y = ev->y;
        v->drag_pan_x   = v->pan_x;
        v->drag_pan_y   = v->pan_y;
    }
}

static void on_mouse_up(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win; (void)ev;
    Viewer *v = (Viewer *)userdata;
    v->drag_active = 0;
}

static void on_mouse_move(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    Viewer *v = (Viewer *)userdata;
    if (!v->drag_active || !v->img) return;
    v->pan_x = v->drag_pan_x + (ev->x - v->drag_start_x);
    v->pan_y = v->drag_pan_y + (ev->y - v->drag_start_y);
    v->fit_mode = 0;
    nv_surface_invalidate(v->surface);
}

static void on_scroll(NvWindow *win, NvScrollEvent *ev, void *userdata) {
    (void)win;
    Viewer *v = (Viewer *)userdata;
    if (!v->img) return;

    v->fit_mode = 0;
    if (ev->delta_y < 0)
        viewer_zoom_at(v, v->zoom * ZOOM_STEP, ev->x, ev->y);
    else
        viewer_zoom_at(v, v->zoom / ZOOM_STEP, ev->x, ev->y);

    snprintf(v->status, sizeof(v->status),
             "%d x %d  %s  %.0f%%",
             v->img->width, v->img->height,
             format_name(v->img->format), v->zoom * 100.0f);

    nv_surface_invalidate(v->surface);
}

static void on_key_down(NvWindow *win, NvKeyEvent *ev, void *userdata) {
    (void)win;
    Viewer *v = (Viewer *)userdata;

    switch (ev->key) {
        case 'o':
        case 'O':
            viewer_open_dialog(v);
            break;
        case '+':
        case '=':
            if (v->img) { v->fit_mode = 0; viewer_zoom_in(v);  }
            break;
        case '-':
            if (v->img) { v->fit_mode = 0; viewer_zoom_out(v); }
            break;
        case '1':
            if (v->img) { v->fit_mode = 0; viewer_zoom_1to1(v); }
            break;
        case 'f':
        case 'F':
            if (v->img) { v->fit_mode = 1; viewer_fit(v); }
            break;
        case 'r':
        case 'R':
            viewer_rotate_cw(v);
            break;
        case NV_KEY_LEFT:
            v->pan_x -= 32; v->fit_mode = 0;
            nv_surface_invalidate(v->surface);
            break;
        case NV_KEY_RIGHT:
            v->pan_x += 32; v->fit_mode = 0;
            nv_surface_invalidate(v->surface);
            break;
        case NV_KEY_UP:
            v->pan_y -= 32; v->fit_mode = 0;
            nv_surface_invalidate(v->surface);
            break;
        case NV_KEY_DOWN:
            v->pan_y += 32; v->fit_mode = 0;
            nv_surface_invalidate(v->surface);
            break;
        case NV_KEY_ESCAPE:
            if (v->img) { v->fit_mode = 1; viewer_fit(v); }
            break;
        default:
            break;
    }

    if (v->img) {
        snprintf(v->status, sizeof(v->status),
                 "%d x %d  %s  %.0f%%",
                 v->img->width, v->img->height,
                 format_name(v->img->format), v->zoom * 100.0f);
        nv_surface_invalidate(v->surface);
    }
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(int argc, char **argv) {
    memset(&g_viewer, 0, sizeof(Viewer));
    g_viewer.zoom   = 1.0f;
    g_viewer.fit_mode = 1;
    strncpy(g_viewer.status, "No image loaded", sizeof(g_viewer.status) - 1);

    NvWindowConfig cfg;
    cfg.title    = "Image Viewer";
    cfg.width    = WINDOW_W;
    cfg.height   = WINDOW_H;
    cfg.resizable = 0;

    g_viewer.window = nv_window_create(&cfg);
    if (!g_viewer.window) return 1;

    nv_window_on_paint(g_viewer.window,      on_paint,      &g_viewer);
    nv_window_on_mouse_down(g_viewer.window, on_mouse_down, &g_viewer);
    nv_window_on_mouse_up(g_viewer.window,   on_mouse_up,   &g_viewer);
    nv_window_on_mouse_move(g_viewer.window, on_mouse_move, &g_viewer);
    nv_window_on_scroll(g_viewer.window,     on_scroll,     &g_viewer);
    nv_window_on_key_down(g_viewer.window,   on_key_down,   &g_viewer);

    if (argc > 1) viewer_open(&g_viewer, argv[1]);

    return nv_app_run();
}