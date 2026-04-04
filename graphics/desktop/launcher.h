#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "../server/compositor.h"
#include "../wm/wm.h"
#include "../draw/draw.h"
#include "../font/font_render.h"
#include <stdint.h>

#define LAUNCHER_W           320
#define LAUNCHER_ITEM_H      44
#define LAUNCHER_PADDING     10
#define LAUNCHER_SEARCH_H    36
#define LAUNCHER_MAX_ITEMS   32
#define LAUNCHER_MAX_VISIBLE 8
#define LAUNCHER_ICON_SIZE   28

typedef struct {
    char     name[64];
    char     exec[256];
    char     category[32];
    uint32_t icon_color;
    int      hovered;
} LauncherItem;

typedef struct Launcher Launcher;

Launcher   *launcher_new(Compositor *comp, WindowManager *wm);
void        launcher_free(Launcher *l);

void        launcher_show(Launcher *l, int x, int y);
void        launcher_hide(Launcher *l);
void        launcher_toggle(Compositor *comp, WindowManager *wm, int x, int y);
int         launcher_visible(Launcher *l);

void        launcher_draw(Launcher *l, NvSurface *surface);
void        launcher_mouse_down(Launcher *l, int x, int y);
void        launcher_mouse_move(Launcher *l, int x, int y);
int         launcher_key_down(Launcher *l, int key, uint32_t codepoint);

void        launcher_add_item(Launcher *l, const char *name,
                              const char *exec, const char *category,
                              uint32_t icon_color);
void        launcher_load_defaults(Launcher *l);

#endif