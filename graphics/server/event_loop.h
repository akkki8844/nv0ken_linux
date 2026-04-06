#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include "compositor.h"
#include "ipc_server.h"
#include "display.h"
#include <stdint.h>

#define EVENT_LOOP_TARGET_FPS   60
#define EVENT_LOOP_FRAME_MS     (1000 / EVENT_LOOP_TARGET_FPS)

typedef enum {
    EV_KEY_DOWN,
    EV_KEY_UP,
    EV_MOUSE_MOVE,
    EV_MOUSE_DOWN,
    EV_MOUSE_UP,
    EV_SCROLL,
    EV_TIMER,
    EV_IPC,
    EV_QUIT,
} EventType;

typedef struct {
    EventType type;
    union {
        struct { int x, y; uint32_t button; uint32_t mods; } mouse;
        struct { int dx, dy; } scroll;
        struct { uint32_t key; uint32_t codepoint; uint32_t mods; } key;
        struct { int id; } timer;
    };
} Event;

typedef void (*event_handler_t)(const Event *ev, void *userdata);
typedef void (*frame_handler_t)(void *userdata);

typedef struct EventLoop EventLoop;

EventLoop  *event_loop_new(Display *display, IpcServer *ipc);
void        event_loop_free(EventLoop *el);

void        event_loop_set_event_handler(EventLoop *el,
                                          event_handler_t handler,
                                          void *userdata);
void        event_loop_set_frame_handler(EventLoop *el,
                                          frame_handler_t handler,
                                          void *userdata);

int         event_loop_add_timer(EventLoop *el, int interval_ms);
void        event_loop_remove_timer(EventLoop *el, int timer_id);
void        event_loop_quit(EventLoop *el);
void        event_loop_run(EventLoop *el);
int         event_loop_running(const EventLoop *el);
long long   event_loop_frame_count(const EventLoop *el);

int         event_loop_mouse_x(const EventLoop *el);
int         event_loop_mouse_y(const EventLoop *el);
uint32_t    event_loop_mouse_buttons(const EventLoop *el);
uint32_t    event_loop_modifiers(const EventLoop *el);

#endif