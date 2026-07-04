#ifndef NV0_INPUT_H
#define NV0_INPUT_H

#include <stdint.h>

enum {
    NV_KEY_UNKNOWN = 0,
    NV_KEY_ESCAPE = 27,
    NV_KEY_BACKSPACE = 8,
    NV_KEY_TAB = 9,
    NV_KEY_RETURN = 13,
    NV_KEY_DELETE = 127,
    NV_KEY_INSERT = 1001,
    NV_KEY_HOME,
    NV_KEY_END,
    NV_KEY_PAGE_UP,
    NV_KEY_PAGE_DOWN,
    NV_KEY_LEFT,
    NV_KEY_RIGHT,
    NV_KEY_UP,
    NV_KEY_DOWN,
    NV_KEY_F1,
    NV_KEY_F2,
    NV_KEY_F3,
    NV_KEY_F4,
    NV_KEY_F5,
    NV_KEY_F6,
    NV_KEY_F7,
    NV_KEY_F8,
    NV_KEY_F9,
    NV_KEY_F10,
    NV_KEY_F11,
    NV_KEY_F12,
    NV_KEY_ALT_LEFT,
    NV_KEY_ALT_RIGHT,
};

enum {
    NV_MOD_SHIFT = 1u << 0,
    NV_MOD_CTRL  = 1u << 1,
    NV_MOD_ALT   = 1u << 2,
    NV_MOD_SUPER = 1u << 3,
};

enum {
    NV_MOUSE_LEFT = 1u,
    NV_MOUSE_RIGHT = 2u,
    NV_MOUSE_MIDDLE = 3u,
};

typedef struct {
    int x;
    int y;
    uint32_t button;
    union { uint32_t modifiers; uint32_t mod; };
} NvMouseEvent;

typedef struct {
    uint32_t key;
    uint32_t codepoint;
    union { uint32_t modifiers; uint32_t mod; };
} NvKeyEvent;

typedef struct {
    int x;
    int y;
    int delta_x;
    int delta_y;
} NvScrollEvent;

#endif
