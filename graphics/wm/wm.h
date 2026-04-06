#ifndef WM_H
#define WM_H

#include "../server/compositor.h"
#include "../server/ipc_server.h"
#include "../draw/draw.h"
#include <stdint.h>

#define WM_MAX_WINDOWS       64
#define WM_MIN_WIDTH         120
#define WM_MIN_HEIGHT        80
#define WM_TITLEBAR_H        28
#define WM_BORDER_W          4
#define WM_RESIZE_HANDLE     8
#define WM_SNAP_THRESHOLD    16
#define WM_ANIMATE_STEPS     8

typedef enum {
    WIN_STATE_NORMAL,
    WIN_STATE_MINIMIZED,
    WIN_STATE_MAXIMIZED,
    WIN_STATE_FULLSCREEN,
} WindowState;

typedef enum {
    WIN_FLAG_RESIZABLE   = 0x01,
    WIN_FLAG_DECORATED   = 0x02,
    WIN_FLAG_ALWAYS_TOP  = 0x04,
    WIN_FLAG_NO_FOCUS    = 0x08,
    WIN_FLAG_FULLSCREEN  = 0x10,
    WIN_FLAG_MODAL       = 0x20,
} WindowFlags;

typedef enum {
    HIT_NONE,
    HIT_TITLEBAR,
    HIT_CLIENT,
    HIT_RESIZE_N,
    HIT_RESIZE_S,
    HIT_RESIZE_E,
    HIT_RESIZE_W,
    HIT_RESIZE_NE,
    HIT_RESIZE_NW,
    HIT_RESIZE_SE,
    HIT_RESIZE_SW,
    HIT_CLOSE,
    HIT_MINIMIZE,
    HIT_MAXIMIZE,
} HitZone;

typedef struct Window Window;

struct Window {
    int          id;
    char         title[128];
    int          x, y, w, h;
    int          client_x, client_y, client_w, client_h;
    int          restore_x, restore_y, restore_w, restore_h;
    WindowState  state;
    int          flags;
    int          focused;
    int          visible;
    int          comp_surface_id;
    int          frame_surface_id;
    IpcClient   *owner;
    Window      *modal_parent;
    int          z_order;
};

typedef struct WindowManager WindowManager;

WindowManager *wm_new(Compositor *comp, int screen_w, int screen_h);
void           wm_free(WindowManager *wm);

Window *wm_create_window(WindowManager *wm, IpcClient *owner,
                          int x, int y, int w, int h,
                          const char *title, int flags);
void    wm_destroy_window(WindowManager *wm, int win_id);
Window *wm_find_window(WindowManager *wm, int win_id);

void    wm_focus_window(WindowManager *wm, int win_id);
void    wm_minimize_window(WindowManager *wm, int win_id);
void    wm_restore_window(WindowManager *wm, int win_id);
void    wm_maximize_window(WindowManager *wm, int win_id);
void    wm_fullscreen_window(WindowManager *wm, int win_id);
void    wm_close_window(WindowManager *wm, int win_id);

void    wm_set_title(WindowManager *wm, int win_id, const char *title);
void    wm_resize_window(WindowManager *wm, int win_id, int w, int h);
void    wm_move_window(WindowManager *wm, int win_id, int x, int y);
void    wm_show_window(WindowManager *wm, int win_id);
void    wm_hide_window(WindowManager *wm, int win_id);

void    wm_raise_window(WindowManager *wm, int win_id);
void    wm_lower_window(WindowManager *wm, int win_id);

HitZone wm_hit_test(WindowManager *wm, int x, int y, int *out_win_id);

void    wm_mouse_down(WindowManager *wm, int x, int y, int button,
                      uint32_t mods);
void    wm_mouse_up(WindowManager *wm, int x, int y, int button,
                    uint32_t mods);
void    wm_mouse_move(WindowManager *wm, int x, int y, uint32_t mods);
void    wm_key_down(WindowManager *wm, uint32_t key, uint32_t cp,
                    uint32_t mods);
void    wm_key_up(WindowManager *wm, uint32_t key, uint32_t cp,
                  uint32_t mods);

void    wm_draw_all(WindowManager *wm, NvSurface *surface);
int     wm_window_count(WindowManager *wm);
int     wm_focused_window(WindowManager *wm);
Window *wm_get_window_at(WindowManager *wm, int x, int y);

#endif