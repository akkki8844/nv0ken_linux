#ifndef TASKBAR_H
#define TASKBAR_H

#include "../server/compositor.h"
#include "../wm/wm.h"
#include "../draw/draw.h"
#include "../font/font_render.h"
#include <stdint.h>

#define TASKBAR_HEIGHT       36
#define TASKBAR_BTN_W        140
#define TASKBAR_BTN_H        28
#define TASKBAR_BTN_MARGIN   4
#define TASKBAR_BTN_PAD_X    10
#define TASKBAR_START_W      60
#define TASKBAR_CLOCK_W      70
#define TASKBAR_TRAY_ITEM_W  24
#define TASKBAR_MAX_BTNS     24
#define TASKBAR_MAX_TRAY     8

typedef enum {
    TRAY_NETWORK,
    TRAY_VOLUME,
    TRAY_BATTERY,
    TRAY_CUSTOM,
} TrayItemType;

typedef struct {
    TrayItemType type;
    char         tooltip[64];
    uint32_t     icon_color;
    int          active;
    void       (*on_click)(void *userdata);
    void        *userdata;
} TrayItem;

typedef struct {
    int      window_id;
    char     title[128];
    int      focused;
    int      minimized;
    int      x;
    int      w;
} TaskBtn;

typedef struct Taskbar Taskbar;

Taskbar    *taskbar_new(Compositor *comp, WindowManager *wm,
                        int screen_w, int screen_h);
void        taskbar_free(Taskbar *t);

void        taskbar_draw(Taskbar *t, NvSurface *surface);
void        taskbar_mouse_down(Taskbar *t, int x, int y, int button);
void        taskbar_mouse_move(Taskbar *t, int x, int y);
void        taskbar_mouse_up(Taskbar *t, int x, int y);

void        taskbar_add_window(Taskbar *t, int win_id, const char *title);
void        taskbar_remove_window(Taskbar *t, int win_id);
void        taskbar_update_window(Taskbar *t, int win_id, const char *title,
                                  int focused, int minimized);

void        taskbar_add_tray(Taskbar *t, TrayItem *item);
void        taskbar_tick(Taskbar *t);

int         taskbar_height(void);
int         taskbar_y(Taskbar *t);

#endif