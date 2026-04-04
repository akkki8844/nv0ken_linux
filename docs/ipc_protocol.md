# IPC protocol

## overview

the compositor (`graphics/server/compositor.c`) and window manager communicate with userspace applications through a Unix-domain-socket-style IPC channel. each application that wants a window connects to the compositor's socket, sends typed messages, and receives responses.

the socket path is `/tmp/nv0_compositor`.

## message format

all messages start with a fixed 8-byte header:

```c
typedef struct {
    uint32_t type;
    uint32_t length;
} IpcHeader;
```

`length` is the byte count of the payload that follows the header. the receiver reads the 8-byte header first, then reads exactly `length` more bytes.

## message types

### client → compositor

| type | value | payload struct         | description                    |
|------|-------|------------------------|--------------------------------|
| NV_MSG_CREATE_WINDOW    | 1  | IpcCreateWindow    | create a new window            |
| NV_MSG_DESTROY_WINDOW   | 2  | IpcWindowId        | destroy a window               |
| NV_MSG_SET_TITLE        | 3  | IpcSetTitle        | set window title string        |
| NV_MSG_RESIZE           | 4  | IpcResize          | request window resize          |
| NV_MSG_MOVE             | 5  | IpcMove            | move window to position        |
| NV_MSG_SHOW             | 6  | IpcWindowId        | make window visible            |
| NV_MSG_HIDE             | 7  | IpcWindowId        | hide window                    |
| NV_MSG_INVALIDATE       | 8  | IpcWindowId        | request repaint                |
| NV_MSG_SWAP_BUFFER      | 9  | IpcSwapBuffer      | present rendered frame         |
| NV_MSG_SET_CURSOR       | 10 | IpcSetCursor       | set cursor shape for window    |
| NV_MSG_CLIPBOARD_SET    | 11 | IpcClipboard       | write text to clipboard        |
| NV_MSG_CLIPBOARD_GET    | 12 | IpcWindowId        | request clipboard text         |

### compositor → client

| type | value | payload struct         | description                    |
|------|-------|------------------------|--------------------------------|
| NV_MSG_ACK              | 100 | IpcAck             | general acknowledgement        |
| NV_MSG_PAINT            | 101 | IpcPaint           | compositor requests redraw     |
| NV_MSG_KEY_DOWN         | 102 | IpcKeyEvent        | key pressed                    |
| NV_MSG_KEY_UP           | 103 | IpcKeyEvent        | key released                   |
| NV_MSG_MOUSE_DOWN       | 104 | IpcMouseEvent      | mouse button pressed           |
| NV_MSG_MOUSE_UP         | 105 | IpcMouseEvent      | mouse button released          |
| NV_MSG_MOUSE_MOVE       | 106 | IpcMouseEvent      | mouse moved                    |
| NV_MSG_SCROLL           | 107 | IpcScrollEvent     | scroll wheel                   |
| NV_MSG_FOCUS_IN         | 108 | IpcWindowId        | window gained focus            |
| NV_MSG_FOCUS_OUT        | 109 | IpcWindowId        | window lost focus              |
| NV_MSG_CLOSE_REQUEST    | 110 | IpcWindowId        | user clicked close button      |
| NV_MSG_RESIZE_NOTIFY    | 111 | IpcResize          | compositor resized window      |
| NV_MSG_CLIPBOARD_DATA   | 112 | IpcClipboard       | clipboard text response        |
| NV_MSG_ERROR            | 200 | IpcError           | error response                 |

## payload structs

```c
typedef struct { uint32_t window_id; } IpcWindowId;

typedef struct {
    uint32_t x, y, w, h;
    uint32_t flags;
    char     title[128];
} IpcCreateWindow;

typedef struct {
    uint32_t window_id;
    char     title[128];
} IpcSetTitle;

typedef struct {
    uint32_t window_id;
    uint32_t w, h;
} IpcResize;

typedef struct {
    uint32_t window_id;
    int32_t  x, y;
} IpcMove;

typedef struct {
    uint32_t window_id;
    uint32_t x, y, w, h;
} IpcPaint;

typedef struct {
    uint32_t window_id;
    uint32_t shm_id;
    uint32_t w, h;
} IpcSwapBuffer;

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

typedef struct {
    uint32_t window_id;
    int32_t  delta_x;
    int32_t  delta_y;
} IpcScrollEvent;

typedef struct {
    uint32_t cursor_type;
} IpcSetCursor;

typedef struct {
    uint32_t window_id;
    uint32_t ok;
} IpcAck;

typedef struct {
    uint32_t window_id;
    int32_t  errno_val;
    char     message[64];
} IpcError;

typedef struct {
    uint32_t length;
    char     text[1];
} IpcClipboard;
```

## window flags

```c
#define NV_WINDOW_RESIZABLE    (1 << 0)
#define NV_WINDOW_DECORATED    (1 << 1)
#define NV_WINDOW_ALWAYS_TOP   (1 << 2)
#define NV_WINDOW_NO_FOCUS     (1 << 3)
#define NV_WINDOW_FULLSCREEN   (1 << 4)
```

## modifier keys

```c
#define NV_MOD_SHIFT   (1 << 0)
#define NV_MOD_CTRL    (1 << 1)
#define NV_MOD_ALT     (1 << 2)
#define NV_MOD_SUPER   (1 << 3)
```

## shared memory buffer protocol

when an application calls `nv_surface_invalidate`, it renders into a shared memory buffer identified by `shm_id`. it then sends `NV_MSG_SWAP_BUFFER` with the dimensions. the compositor reads the pixels from the shared memory region and blits them into the window's surface on its next composite pass.

the shared memory region is allocated via `sys_shmget` with a key derived from the window ID. the compositor and client both call `sys_shmat` to map it.

## cursor types

```c
#define NV_CURSOR_ARROW    0
#define NV_CURSOR_IBEAM    1
#define NV_CURSOR_RESIZE   2
#define NV_CURSOR_WAIT     3
#define NV_CURSOR_HIDDEN   4
```

## connection lifecycle

```
client                          compositor
  |                                  |
  |-- connect(/tmp/nv0_compositor) ->|
  |                                  |
  |-- NV_MSG_CREATE_WINDOW --------->|
  |<- NV_MSG_ACK (window_id=N) ------|
  |                                  |
  |   [compositor sends events]      |
  |<- NV_MSG_PAINT ------------------|
  |-- NV_MSG_SWAP_BUFFER ----------->|
  |                                  |
  |<- NV_MSG_KEY_DOWN ---------------|
  |<- NV_MSG_MOUSE_DOWN -------------|
  |                                  |
  |<- NV_MSG_CLOSE_REQUEST ----------|
  |-- NV_MSG_DESTROY_WINDOW -------->|
  |-- disconnect ------------------->|
```