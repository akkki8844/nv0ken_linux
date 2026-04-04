#ifndef DESKTOP_H
#define DESKTOP_H

#include "../server/compositor.h"
#include "../wm/wm.h"
#include "../draw/draw.h"
#include "../font/font_render.h"
#include <stdint.h>

#define DESKTOP_ICON_SIZE       64
#define DESKTOP_ICON_LABEL_H    20
#define DESKTOP_ICON_GRID_X     24
#define DESKTOP_ICON_GRID_Y     24
#define DESKTOP_ICON_SPACING_X  96
#define DESKTOP_ICON_SPACING_Y  100
#define DESKTOP_MAX_ICONS       64
#define DESKTOP_ICONS_PER_COL   8

typedef enum {
    ICON_APP,
    ICON_FILE,
    ICON_FOLDER,
    ICON_LINK,
} IconType;

typedef struct {
    char      label[64];
    char      exec[256];
    char      icon_path[256];
    uint32_t *icon_pixels;
    int       icon_w;
    int       icon_h;
    int       x;
    int       y;
    IconType  type;
    int       selected;
    int       hovered;
} DesktopIcon;

typedef struct {
    int          x;
    int          y;
    int          w;
    int          h;
    int          visible;
    char         items[16][64];
    char         actions[16][64];
    int          item_count;
    int          hovered_item;
} ContextMenu;

typedef struct Desktop Desktop;

Desktop     *desktop_new(Compositor *comp, WindowManager *wm,
                         int x, int y, int w, int h);
void         desktop_free(Desktop *d);

void         desktop_load_icons(Desktop *d);
void         desktop_add_icon(Desktop *d, const char *label,
                              const char *exec, const char *icon_path,
                              IconType type);

void         desktop_draw(Desktop *d, NvSurface *surface);
void         desktop_mouse_down(Desktop *d, int x, int y, int button);
void         desktop_mouse_up(Desktop *d, int x, int y, int button);
void         desktop_mouse_move(Desktop *d, int x, int y);
void         desktop_double_click(Desktop *d, int x, int y);
void         desktop_key_down(Desktop *d, int key, int mods);

void         desktop_set_wallpaper(Desktop *d, const char *path);
void         desktop_refresh(Desktop *d);

#endif