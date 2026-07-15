#include "../include/nv0/window.h"

#include <stdlib.h>
#include <string.h>

#include "../include/nv0/ipc.h"
#include "../../graphics/server/ipc_server.h"
#include "internal.h"

NvWindow *nv_windows[NV0_MAX_WINDOWS];
int nv_window_count;
int nv_running;

static int next_window_id = 1;

static void copy_title(char *dest, size_t size, const char *title)
{
    if (!dest || size == 0) {
        return;
    }
    if (!title) {
        title = "nv0";
    }
    strncpy(dest, title, size - 1);
    dest[size - 1] = '\0';
}

NvWindow *nv_window_find(int id)
{
    for (int i = 0; i < nv_window_count; ++i) {
        if (nv_windows[i] && nv_windows[i]->id == id) {
            return nv_windows[i];
        }
    }
    return 0;
}

NvWindow *nv_window_create(const NvWindowConfig *config)
{
    if (!config || config->width <= 0 || config->height <= 0 ||
        nv_window_count >= NV0_MAX_WINDOWS) {
        return 0;
    }

    NvWindow *window = calloc(1, sizeof(*window));
    if (!window) {
        return 0;
    }

    window->surface = surface_new(config->width, config->height);
    if (!window->surface) {
        free(window);
        return 0;
    }

    window->id = next_window_id++;
    window->width = config->width;
    window->height = config->height;
    window->x = config->x;
    window->y = config->y;
    window->visible = 1;
    window->dirty = 1;
    copy_title(window->title, sizeof(window->title), config->title);

    nv_windows[nv_window_count++] = window;

    IpcCreateWindow msg;
    memset(&msg, 0, sizeof(msg));
    msg.x = (uint32_t)window->x;
    msg.y = (uint32_t)window->y;
    msg.w = (uint32_t)window->width;
    msg.h = (uint32_t)window->height;
    msg.flags = config->flags | (config->resizable ? 1u : 0u);
    copy_title(msg.title, sizeof(msg.title), window->title);
    nv_ipc_send(NV_MSG_CREATE_WINDOW, &msg, sizeof(msg));

    return window;
}

void nv_window_destroy(NvWindow *window)
{
    if (!window) {
        return;
    }

    IpcWindowId msg = {(uint32_t)window->id};
    nv_ipc_send(NV_MSG_DESTROY_WINDOW, &msg, sizeof(msg));

    for (int i = 0; i < nv_window_count; ++i) {
        if (nv_windows[i] == window) {
            memmove(&nv_windows[i], &nv_windows[i + 1],
                    (size_t)(nv_window_count - i - 1) * sizeof(nv_windows[0]));
            --nv_window_count;
            break;
        }
    }

    surface_free(window->surface);
    free(window);

    if (nv_window_count == 0) {
        nv_running = 0;
    }
}

void nv_window_set_title(NvWindow *window, const char *title)
{
    if (!window) {
        return;
    }
    copy_title(window->title, sizeof(window->title), title);

    IpcSetTitle msg;
    memset(&msg, 0, sizeof(msg));
    msg.window_id = (uint32_t)window->id;
    copy_title(msg.title, sizeof(msg.title), window->title);
    nv_ipc_send(NV_MSG_SET_TITLE, &msg, sizeof(msg));
}

void nv_window_show(NvWindow *window)
{
    if (!window) {
        return;
    }
    window->visible = 1;
    IpcWindowId msg = {(uint32_t)window->id};
    nv_ipc_send(NV_MSG_SHOW, &msg, sizeof(msg));
}

void nv_window_hide(NvWindow *window)
{
    if (!window) {
        return;
    }
    window->visible = 0;
    IpcWindowId msg = {(uint32_t)window->id};
    nv_ipc_send(NV_MSG_HIDE, &msg, sizeof(msg));
}

void nv_window_close(NvWindow *window)
{
    if (!window) {
        return;
    }
    if (window->on_close) {
        window->on_close(window, window->close_userdata);
    }
    nv_window_destroy(window);
}

void nv_close(NvWindow *window)
{
    nv_window_close(window);
}

NvSurface *nv_window_surface(NvWindow *window)
{
    return window ? window->surface : 0;
}

int nv_window_width(const NvWindow *window)
{
    return window ? window->width : 0;
}

int nv_window_height(const NvWindow *window)
{
    return window ? window->height : 0;
}

int nv_window_id(const NvWindow *window)
{
    return window ? window->id : 0;
}

void nv_window_mark_dirty(NvWindow *window)
{
    if (window) {
        window->dirty = 1;
    }
}

void nv_window_paint_if_dirty(NvWindow *window)
{
    if (!window || !window->dirty || !window->on_paint) {
        return;
    }
    window->dirty = 0;
    window->on_paint(window, window->surface, window->paint_userdata);
    nv_window_flush(window);
}

int nv_window_flush(NvWindow *window)
{
    if (!window) {
        return -1;
    }

    IpcWindowId invalidate = {(uint32_t)window->id};
    return nv_ipc_send(NV_MSG_INVALIDATE, &invalidate, sizeof(invalidate));
}

int nv_window_dispatch(uint32_t type, const void *payload, uint32_t length)
{
    if (!payload) {
        return -1;
    }

    if (type == NV_MSG_PAINT && length >= sizeof(IpcPaint)) {
        const IpcPaint *paint = payload;
        NvWindow *window = nv_window_find((int)paint->window_id);
        nv_window_mark_dirty(window);
        return 0;
    }
    if ((type == NV_MSG_KEY_DOWN || type == NV_MSG_KEY_UP) &&
        length >= sizeof(IpcKeyEvent)) {
        const IpcKeyEvent *key = payload;
        NvWindow *window = nv_window_find((int)key->window_id);
        if (!window) {
            return -1;
        }
        NvKeyEvent event = {
            .key = key->key,
            .codepoint = key->codepoint,
            .modifiers = key->modifiers,
        };
        if (type == NV_MSG_KEY_DOWN && window->on_key_down) {
            window->on_key_down(window, &event, window->key_down_userdata);
        } else if (type == NV_MSG_KEY_UP && window->on_key_up) {
            window->on_key_up(window, &event, window->key_up_userdata);
        }
        return 0;
    }
    if ((type == NV_MSG_MOUSE_DOWN || type == NV_MSG_MOUSE_UP ||
         type == NV_MSG_MOUSE_MOVE) && length >= sizeof(IpcMouseEvent)) {
        const IpcMouseEvent *mouse = payload;
        NvWindow *window = nv_window_find((int)mouse->window_id);
        if (!window) {
            return -1;
        }
        NvMouseEvent event = {
            .x = mouse->x,
            .y = mouse->y,
            .button = mouse->button,
            .modifiers = mouse->modifiers,
        };
        if (type == NV_MSG_MOUSE_DOWN && window->on_mouse_down) {
            window->on_mouse_down(window, &event, window->mouse_down_userdata);
        } else if (type == NV_MSG_MOUSE_UP && window->on_mouse_up) {
            window->on_mouse_up(window, &event, window->mouse_up_userdata);
        } else if (type == NV_MSG_MOUSE_MOVE && window->on_mouse_move) {
            window->on_mouse_move(window, &event, window->mouse_move_userdata);
        }
        return 0;
    }
    if (type == NV_MSG_SCROLL && length >= sizeof(IpcScrollEvent)) {
        const IpcScrollEvent *scroll = payload;
        NvWindow *window = nv_window_find((int)scroll->window_id);
        if (window && window->on_scroll) {
            NvScrollEvent event = {
                .x = window->x,
                .y = window->y,
                .delta_x = scroll->delta_x,
                .delta_y = scroll->delta_y,
            };
            window->on_scroll(window, &event, window->scroll_userdata);
        }
        return 0;
    }
    if (type == NV_MSG_RESIZE_NOTIFY && length >= sizeof(IpcResize)) {
        const IpcResize *resize = payload;
        NvWindow *window = nv_window_find((int)resize->window_id);
        if (window && ((int)resize->w != window->width || (int)resize->h != window->height)) {
            window->width = (int)resize->w;
            window->height = (int)resize->h;
            surface_free(window->surface);
            window->surface = surface_new(window->width, window->height);
            nv_window_mark_dirty(window);
        }
        return 0;
    }
    if (type == NV_MSG_CLOSE_REQUEST && length >= sizeof(IpcWindowId)) {
        const IpcWindowId *close_msg = payload;
        nv_window_close(nv_window_find((int)close_msg->window_id));
        return 0;
    }

    return 0;
}

void nv_window_on_paint(NvWindow *window, NvPaintCallback cb, void *userdata)
{
    if (window) {
        window->on_paint = cb;
        window->paint_userdata = userdata;
        nv_window_mark_dirty(window);
    }
}

void nv_window_on_mouse_down(NvWindow *window, NvMouseCallback cb, void *userdata)
{
    if (window) {
        window->on_mouse_down = cb;
        window->mouse_down_userdata = userdata;
    }
}

void nv_window_on_mouse_up(NvWindow *window, NvMouseCallback cb, void *userdata)
{
    if (window) {
        window->on_mouse_up = cb;
        window->mouse_up_userdata = userdata;
    }
}

void nv_window_on_mouse_move(NvWindow *window, NvMouseCallback cb, void *userdata)
{
    if (window) {
        window->on_mouse_move = cb;
        window->mouse_move_userdata = userdata;
    }
}

void nv_window_on_key_down(NvWindow *window, NvKeyCallback cb, void *userdata)
{
    if (window) {
        window->on_key_down = cb;
        window->key_down_userdata = userdata;
    }
}

void nv_window_on_key_up(NvWindow *window, NvKeyCallback cb, void *userdata)
{
    if (window) {
        window->on_key_up = cb;
        window->key_up_userdata = userdata;
    }
}

void nv_window_on_scroll(NvWindow *window, NvScrollCallback cb, void *userdata)
{
    if (window) {
        window->on_scroll = cb;
        window->scroll_userdata = userdata;
    }
}

void nv_window_on_close(NvWindow *window, NvCloseCallback cb, void *userdata)
{
    if (window) {
        window->on_close = cb;
        window->close_userdata = userdata;
    }
}
