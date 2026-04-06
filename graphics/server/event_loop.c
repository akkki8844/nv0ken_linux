#include "event_loop.h"
#include "display.h"
#include "ipc_server.h"
#include "cursor.h"
#include <stdlib.h>
#include <string.h>

#define MAX_TIMERS 16
#define PS2_KB_PORT   0x60
#define PS2_STS_PORT  0x64

/* -----------------------------------------------------------------------
 * PS/2 keyboard scancode → key/codepoint translation (Set 1)
 * --------------------------------------------------------------------- */

#define MOD_SHIFT  0x01
#define MOD_CTRL   0x02
#define MOD_ALT    0x04
#define MOD_SUPER  0x08

static const uint32_t sc_to_key[128] = {
    [0x01]=NV_KEY_ESCAPE,  [0x02]='1', [0x03]='2', [0x04]='3', [0x05]='4',
    [0x06]='5', [0x07]='6', [0x08]='7', [0x09]='8', [0x0A]='9', [0x0B]='0',
    [0x0C]='-', [0x0D]='=', [0x0E]=NV_KEY_BACKSPACE,
    [0x0F]=NV_KEY_TAB,
    [0x10]='q', [0x11]='w', [0x12]='e', [0x13]='r', [0x14]='t',
    [0x15]='y', [0x16]='u', [0x17]='i', [0x18]='o', [0x19]='p',
    [0x1A]='[', [0x1B]=']', [0x1C]=NV_KEY_RETURN,
    [0x1E]='a', [0x1F]='s', [0x20]='d', [0x21]='f', [0x22]='g',
    [0x23]='h', [0x24]='j', [0x25]='k', [0x26]='l',
    [0x27]=';', [0x28]='\'', [0x29]='`', [0x2B]='\\',
    [0x2C]='z', [0x2D]='x', [0x2E]='c', [0x2F]='v', [0x30]='b',
    [0x31]='n', [0x32]='m', [0x33]=',', [0x34]='.', [0x35]='/',
    [0x37]='*',
    [0x39]=' ',
    [0x3B]=NV_KEY_F1,  [0x3C]=NV_KEY_F2,  [0x3D]=NV_KEY_F3,
    [0x3E]=NV_KEY_F4,  [0x3F]=NV_KEY_F5,  [0x40]=NV_KEY_F6,
    [0x41]=NV_KEY_F7,  [0x42]=NV_KEY_F8,  [0x43]=NV_KEY_F9,
    [0x44]=NV_KEY_F10, [0x57]=NV_KEY_F11, [0x58]=NV_KEY_F12,
    [0x48]=NV_KEY_UP,  [0x50]=NV_KEY_DOWN,
    [0x4B]=NV_KEY_LEFT,[0x4D]=NV_KEY_RIGHT,
    [0x47]=NV_KEY_HOME,[0x4F]=NV_KEY_END,
    [0x49]=NV_KEY_PAGE_UP,[0x51]=NV_KEY_PAGE_DOWN,
    [0x52]=NV_KEY_INSERT,[0x53]=NV_KEY_DELETE,
};

static const char sc_unshifted[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,
    0,'q','w','e','r','t','y','u','i','o','p','[',']',0,
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',
    0,' ',
};

static const char sc_shifted[128] = {
    0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,
    0,'Q','W','E','R','T','Y','U','I','O','P','{','}',0,
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',
    0,' ',
};

typedef struct {
    int        id;
    int        interval_ms;
    long long  next_tick;
    int        active;
} Timer;

struct EventLoop {
    Display      *display;
    IpcServer    *ipc;
    CursorMgr    *cursor_mgr;

    event_handler_t  ev_handler;
    void            *ev_userdata;
    frame_handler_t  frame_handler;
    void            *frame_userdata;

    Timer   timers[MAX_TIMERS];
    int     timer_count;
    int     next_timer_id;

    int     mouse_x;
    int     mouse_y;
    int     mouse_buttons;
    uint32_t modifiers;

    int       running;
    long long frame_count;
    long long last_frame_time;

    int     extended_scancode;
};

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

static long long get_ms(void) {
    long long t;
    sys_time(&t);
    return t * 1000;
}

EventLoop *event_loop_new(Display *display, IpcServer *ipc) {
    EventLoop *el = xmalloc(sizeof(EventLoop));
    if (!el) return NULL;
    el->display        = display;
    el->ipc            = ipc;
    el->next_timer_id  = 1;
    el->running        = 0;
    el->mouse_x        = display ? display_width(display)  / 2 : 400;
    el->mouse_y        = display ? display_height(display) / 2 : 300;

    if (display) {
        el->cursor_mgr = cursor_mgr_new(display_compositor(display),
                                         display_width(display),
                                         display_height(display));
    }
    return el;
}

void event_loop_free(EventLoop *el) {
    if (!el) return;
    cursor_mgr_free(el->cursor_mgr);
    free(el);
}

void event_loop_set_event_handler(EventLoop *el,
                                   event_handler_t handler,
                                   void *userdata) {
    if (!el) return;
    el->ev_handler  = handler;
    el->ev_userdata = userdata;
}

void event_loop_set_frame_handler(EventLoop *el,
                                   frame_handler_t handler,
                                   void *userdata) {
    if (!el) return;
    el->frame_handler  = handler;
    el->frame_userdata = userdata;
}

int event_loop_add_timer(EventLoop *el, int interval_ms) {
    if (!el || el->timer_count >= MAX_TIMERS) return -1;
    Timer *t = &el->timers[el->timer_count++];
    t->id          = el->next_timer_id++;
    t->interval_ms = interval_ms;
    t->next_tick   = get_ms() + interval_ms;
    t->active      = 1;
    return t->id;
}

void event_loop_remove_timer(EventLoop *el, int timer_id) {
    if (!el) return;
    for (int i = 0; i < el->timer_count; i++) {
        if (el->timers[i].id == timer_id) {
            el->timers[i].active = 0;
            return;
        }
    }
}

void event_loop_quit(EventLoop *el) {
    if (el) el->running = 0;
}

static void dispatch_ev(EventLoop *el, const Event *ev) {
    if (el->ev_handler) el->ev_handler(ev, el->ev_userdata);
}

/* -----------------------------------------------------------------------
 * PS/2 keyboard poll — reads from the kernel keyboard driver device
 * --------------------------------------------------------------------- */

static void poll_keyboard(EventLoop *el) {
    uint8_t buf[32];
    int fd = open("/dev/kbd", O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;

    ssize_t n = read(fd, buf, sizeof(buf));
    close(fd);
    if (n <= 0) return;

    for (int i = 0; i < (int)n; i++) {
        uint8_t sc = buf[i];

        if (sc == 0xE0) { el->extended_scancode = 1; continue; }

        int released = (sc & 0x80) != 0;
        sc &= 0x7F;

        if (sc == 0x2A || sc == 0x36) {
            if (released) el->modifiers &= ~MOD_SHIFT;
            else          el->modifiers |=  MOD_SHIFT;
            el->extended_scancode = 0;
            continue;
        }
        if (sc == 0x1D) {
            if (released) el->modifiers &= ~MOD_CTRL;
            else          el->modifiers |=  MOD_CTRL;
            el->extended_scancode = 0;
            continue;
        }
        if (sc == 0x38) {
            if (released) el->modifiers &= ~MOD_ALT;
            else          el->modifiers |=  MOD_ALT;
            el->extended_scancode = 0;
            continue;
        }

        uint32_t key = sc_to_key[sc & 0x7F];
        uint32_t cp  = 0;

        if (!key && sc < 128) {
            key = (el->modifiers & MOD_SHIFT) ? sc_shifted[sc] : sc_unshifted[sc];
            cp  = key;
        } else if (key >= 32 && key < 127) {
            cp = key;
        }

        if (!released) {
            Event ev;
            ev.type          = EV_KEY_DOWN;
            ev.key.key       = key;
            ev.key.codepoint = cp;
            ev.key.mods      = el->modifiers;
            dispatch_ev(el, &ev);
        } else {
            Event ev;
            ev.type          = EV_KEY_UP;
            ev.key.key       = key;
            ev.key.codepoint = cp;
            ev.key.mods      = el->modifiers;
            dispatch_ev(el, &ev);
        }

        el->extended_scancode = 0;
    }
}

/* -----------------------------------------------------------------------
 * PS/2 mouse poll — reads from /dev/mouse
 * --------------------------------------------------------------------- */

static void poll_mouse(EventLoop *el) {
    int fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;

    uint8_t pkt[3];
    ssize_t n;
    int dw = el->display ? display_width(el->display)  : 800;
    int dh = el->display ? display_height(el->display) : 600;

    while ((n = read(fd, pkt, 3)) == 3) {
        uint8_t flags = pkt[0];
        int dx =  (int)(int8_t)pkt[1];
        int dy = -(int)(int8_t)pkt[2];

        int prev_x = el->mouse_x;
        int prev_y = el->mouse_y;

        el->mouse_x += dx;
        el->mouse_y += dy;
        if (el->mouse_x < 0)  el->mouse_x = 0;
        if (el->mouse_y < 0)  el->mouse_y = 0;
        if (el->mouse_x >= dw) el->mouse_x = dw - 1;
        if (el->mouse_y >= dh) el->mouse_y = dh - 1;

        if (el->cursor_mgr)
            cursor_mgr_move(el->cursor_mgr, el->mouse_x, el->mouse_y);

        if (el->mouse_x != prev_x || el->mouse_y != prev_y) {
            Event ev;
            ev.type      = EV_MOUSE_MOVE;
            ev.mouse.x   = el->mouse_x;
            ev.mouse.y   = el->mouse_y;
            ev.mouse.button = 0;
            ev.mouse.mods   = el->modifiers;
            dispatch_ev(el, &ev);
        }

        uint32_t prev_btns = el->mouse_buttons;
        uint32_t new_btns  = 0;
        if (flags & 0x01) new_btns |= 1;
        if (flags & 0x02) new_btns |= 4;
        if (flags & 0x04) new_btns |= 2;

        uint32_t pressed  = new_btns & ~prev_btns;
        uint32_t released = prev_btns & ~new_btns;
        el->mouse_buttons = new_btns;

        for (int b = 0; b < 3; b++) {
            if (pressed & (1 << b)) {
                Event ev;
                ev.type         = EV_MOUSE_DOWN;
                ev.mouse.x      = el->mouse_x;
                ev.mouse.y      = el->mouse_y;
                ev.mouse.button = b + 1;
                ev.mouse.mods   = el->modifiers;
                dispatch_ev(el, &ev);
            }
            if (released & (1 << b)) {
                Event ev;
                ev.type         = EV_MOUSE_UP;
                ev.mouse.x      = el->mouse_x;
                ev.mouse.y      = el->mouse_y;
                ev.mouse.button = b + 1;
                ev.mouse.mods   = el->modifiers;
                dispatch_ev(el, &ev);
            }
        }
    }
    close(fd);
}

/* -----------------------------------------------------------------------
 * Timer tick
 * --------------------------------------------------------------------- */

static void poll_timers(EventLoop *el) {
    long long now = get_ms();
    for (int i = 0; i < el->timer_count; i++) {
        Timer *t = &el->timers[i];
        if (!t->active) continue;
        if (now >= t->next_tick) {
            Event ev;
            ev.type     = EV_TIMER;
            ev.timer.id = t->id;
            dispatch_ev(el, &ev);
            t->next_tick = now + t->interval_ms;
        }
    }
}

/* -----------------------------------------------------------------------
 * Main loop
 * --------------------------------------------------------------------- */

void event_loop_run(EventLoop *el) {
    if (!el) return;
    el->running     = 1;
    el->frame_count = 0;

    while (el->running) {
        long long frame_start = get_ms();

        poll_keyboard(el);
        poll_mouse(el);
        poll_timers(el);

        if (el->ipc) ipc_server_poll(el->ipc, 0);

        if (el->frame_handler)
            el->frame_handler(el->frame_userdata);

        if (el->display) display_present(el->display);

        el->frame_count++;
        el->last_frame_time = frame_start;

        long long elapsed = get_ms() - frame_start;
        if (elapsed < EVENT_LOOP_FRAME_MS) {
            unsigned int us = (unsigned int)((EVENT_LOOP_FRAME_MS - elapsed) * 1000);
            usleep(us);
        }
    }
}

int event_loop_running(const EventLoop *el) {
    return el ? el->running : 0;
}

long long event_loop_frame_count(const EventLoop *el) {
    return el ? el->frame_count : 0;
}

int      event_loop_mouse_x(const EventLoop *el) { return el ? el->mouse_x : 0; }
int      event_loop_mouse_y(const EventLoop *el) { return el ? el->mouse_y : 0; }
uint32_t event_loop_mouse_buttons(const EventLoop *el) { return el ? el->mouse_buttons : 0; }
uint32_t event_loop_modifiers(const EventLoop *el)     { return el ? el->modifiers : 0; }