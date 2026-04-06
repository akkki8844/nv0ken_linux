#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include "compositor.h"
#include <stdint.h>

#define IPC_SOCKET_PATH     "/tmp/nv0_compositor"
#define IPC_MAX_CLIENTS     32
#define IPC_RECV_BUF        4096
#define IPC_SEND_BUF        4096

/* message types: client → server */
#define NV_MSG_CREATE_WINDOW    1
#define NV_MSG_DESTROY_WINDOW   2
#define NV_MSG_SET_TITLE        3
#define NV_MSG_RESIZE           4
#define NV_MSG_MOVE             5
#define NV_MSG_SHOW             6
#define NV_MSG_HIDE             7
#define NV_MSG_INVALIDATE       8
#define NV_MSG_SWAP_BUFFER      9
#define NV_MSG_SET_CURSOR       10
#define NV_MSG_CLIPBOARD_SET    11
#define NV_MSG_CLIPBOARD_GET    12

/* message types: server → client */
#define NV_MSG_ACK              100
#define NV_MSG_PAINT            101
#define NV_MSG_KEY_DOWN         102
#define NV_MSG_KEY_UP           103
#define NV_MSG_MOUSE_DOWN       104
#define NV_MSG_MOUSE_UP         105
#define NV_MSG_MOUSE_MOVE       106
#define NV_MSG_SCROLL           107
#define NV_MSG_FOCUS_IN         108
#define NV_MSG_FOCUS_OUT        109
#define NV_MSG_CLOSE_REQUEST    110
#define NV_MSG_RESIZE_NOTIFY    111
#define NV_MSG_CLIPBOARD_DATA   112
#define NV_MSG_ERROR            200

#pragma pack(push, 1)
typedef struct { uint32_t type; uint32_t length; } IpcHeader;

typedef struct { uint32_t window_id; } IpcWindowId;
typedef struct {
    uint32_t x, y, w, h, flags;
    char     title[128];
} IpcCreateWindow;
typedef struct { uint32_t window_id; char title[128]; } IpcSetTitle;
typedef struct { uint32_t window_id; uint32_t w, h; } IpcResize;
typedef struct { uint32_t window_id; int32_t x, y; } IpcMove;
typedef struct { uint32_t window_id; uint32_t x, y, w, h; } IpcPaint;
typedef struct { uint32_t window_id; uint32_t shm_id; uint32_t w, h; } IpcSwapBuffer;
typedef struct {
    uint32_t window_id;
    uint32_t key;
    uint32_t codepoint;
    uint32_t modifiers;
} IpcKeyEvent;
typedef struct {
    uint32_t window_id;
    int32_t  x, y;
    uint32_t button;
    uint32_t modifiers;
} IpcMouseEvent;
typedef struct { uint32_t window_id; int32_t delta_x, delta_y; } IpcScrollEvent;
typedef struct { uint32_t cursor_type; } IpcSetCursor;
typedef struct { uint32_t window_id; uint32_t ok; } IpcAck;
typedef struct {
    uint32_t window_id;
    int32_t  errno_val;
    char     message[64];
} IpcError;
typedef struct { uint32_t length; char text[1]; } IpcClipboard;
#pragma pack(pop)

typedef struct IpcClient {
    int      fd;
    int      active;
    uint8_t  recv_buf[IPC_RECV_BUF];
    int      recv_len;
    uint32_t focused_window;
} IpcClient;

typedef struct IpcServer IpcServer;

typedef void (*ipc_create_window_cb)(IpcServer *srv, IpcClient *client,
                                      const IpcCreateWindow *msg);
typedef void (*ipc_destroy_window_cb)(IpcServer *srv, IpcClient *client,
                                       uint32_t window_id);
typedef void (*ipc_swap_buffer_cb)(IpcServer *srv, IpcClient *client,
                                    const IpcSwapBuffer *msg);
typedef void (*ipc_set_title_cb)(IpcServer *srv, IpcClient *client,
                                  const IpcSetTitle *msg);
typedef void (*ipc_resize_cb)(IpcServer *srv, IpcClient *client,
                               const IpcResize *msg);
typedef void (*ipc_move_cb)(IpcServer *srv, IpcClient *client,
                             const IpcMove *msg);
typedef void (*ipc_show_hide_cb)(IpcServer *srv, IpcClient *client,
                                  uint32_t window_id, int show);
typedef void (*ipc_invalidate_cb)(IpcServer *srv, IpcClient *client,
                                   uint32_t window_id);
typedef void (*ipc_clipboard_cb)(IpcServer *srv, IpcClient *client,
                                  const char *text, int len);

typedef struct {
    ipc_create_window_cb  on_create_window;
    ipc_destroy_window_cb on_destroy_window;
    ipc_swap_buffer_cb    on_swap_buffer;
    ipc_set_title_cb      on_set_title;
    ipc_resize_cb         on_resize;
    ipc_move_cb           on_move;
    ipc_show_hide_cb      on_show_hide;
    ipc_invalidate_cb     on_invalidate;
    ipc_clipboard_cb      on_clipboard_set;
} IpcCallbacks;

IpcServer *ipc_server_new(const IpcCallbacks *callbacks, void *userdata);
void       ipc_server_free(IpcServer *srv);
int        ipc_server_start(IpcServer *srv);
void       ipc_server_stop(IpcServer *srv);
int        ipc_server_poll(IpcServer *srv, int timeout_ms);
void      *ipc_server_userdata(IpcServer *srv);

int  ipc_send_ack(IpcClient *client, uint32_t window_id, int ok);
int  ipc_send_error(IpcClient *client, uint32_t window_id,
                    int errno_val, const char *msg);
int  ipc_send_paint(IpcClient *client, uint32_t window_id,
                    int x, int y, int w, int h);
int  ipc_send_key(IpcClient *client, uint32_t window_id,
                  int type, uint32_t key, uint32_t cp, uint32_t mods);
int  ipc_send_mouse(IpcClient *client, uint32_t window_id,
                    int type, int x, int y, uint32_t btn, uint32_t mods);
int  ipc_send_scroll(IpcClient *client, uint32_t window_id,
                     int dx, int dy);
int  ipc_send_focus(IpcClient *client, uint32_t window_id, int gained);
int  ipc_send_close_request(IpcClient *client, uint32_t window_id);
int  ipc_send_resize_notify(IpcClient *client, uint32_t window_id,
                             int w, int h);
int  ipc_send_clipboard_data(IpcClient *client,
                              const char *text, int len);

#endif