#include "notification.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CLOSE_BTN_SIZE   16

typedef struct {
    NotifType type;
    char      title[64];
    char      body[128];
    int       x, y;
    int       target_y;
    int       age_ms;
    int       alive;
    int       closing;
    int       alpha;
} Notification;

struct NotifManager {
    Notification notifs[NOTIF_MAX];
    int          count;
    int          screen_w;
    int          screen_h;
    int          tick_ms;
};

static const uint32_t NOTIF_ACCENT[] = {
    0x2196F3FF,
    0x4CAF50FF,
    0xFF9800FF,
    0xF44336FF,
};

static const uint32_t NOTIF_BG     = 0x1E1E1EF0;
static const uint32_t NOTIF_FG     = 0xEEEEEEFF;
static const uint32_t NOTIF_DIM    = 0x999999FF;
static const uint32_t NOTIF_BORDER = 0x333333FF;
static const uint32_t CLOSE_FG     = 0x888888FF;
static const uint32_t CLOSE_HOVER  = 0xCCCCCCFF;

NotifManager *notif_manager_new(int screen_w, int screen_h) {
    NotifManager *nm = malloc(sizeof(NotifManager));
    if (!nm) return NULL;
    memset(nm, 0, sizeof(NotifManager));
    nm->screen_w = screen_w;
    nm->screen_h = screen_h;
    return nm;
}

void notif_manager_free(NotifManager *nm) {
    free(nm);
}

static void restack(NotifManager *nm) {
    int slot = 0;
    for (int i = 0; i < nm->count; i++) {
        if (!nm->notifs[i].alive) continue;
        nm->notifs[i].target_y =
            nm->screen_h - 36 - (slot + 1) * (NOTIF_H + NOTIF_MARGIN);
        slot++;
    }
}

void notif_push(NotifManager *nm, NotifType type,
                const char *title, const char *body) {
    if (!nm) return;

    if (nm->count >= NOTIF_MAX) {
        nm->notifs[0].alive = 0;
        memmove(&nm->notifs[0], &nm->notifs[1],
                sizeof(Notification) * (NOTIF_MAX - 1));
        nm->count--;
    }

    Notification *n = &nm->notifs[nm->count++];
    memset(n, 0, sizeof(Notification));
    n->type  = type;
    n->alive = 1;
    n->alpha = 0;
    n->x     = nm->screen_w - NOTIF_W - NOTIF_MARGIN;
    n->y     = nm->screen_h;

    strncpy(n->title, title ? title : "", sizeof(n->title) - 1);
    strncpy(n->body,  body  ? body  : "", sizeof(n->body)  - 1);

    restack(nm);
}

void notif_tick(NotifManager *nm) {
    if (!nm) return;
    nm->tick_ms += 16;

    int changed = 0;
    for (int i = 0; i < nm->count; i++) {
        Notification *n = &nm->notifs[i];
        if (!n->alive) continue;

        n->age_ms += 16;

        int dy = n->target_y - n->y;
        if (dy != 0) {
            n->y += dy / 4 + (dy > 0 ? 1 : -1);
            changed = 1;
        }

        if (n->closing) {
            n->alpha -= 255 / (NOTIF_ANIM_MS / 16);
            if (n->alpha <= 0) {
                n->alive = 0;
                changed  = 1;
            }
        } else {
            if (n->age_ms < NOTIF_ANIM_MS) {
                n->alpha = (n->age_ms * 255) / NOTIF_ANIM_MS;
                changed = 1;
            } else {
                n->alpha = 255;
            }

            if (n->age_ms >= NOTIF_DURATION_MS) {
                n->closing = 1;
                changed    = 1;
            }
        }
    }

    if (changed) {
        int alive = 0;
        for (int i = 0; i < nm->count; i++)
            if (nm->notifs[i].alive) nm->notifs[alive++] = nm->notifs[i];
        nm->count = alive;
        restack(nm);
    }
}

void notif_draw(NotifManager *nm, NvSurface *surface) {
    if (!nm || !surface) return;

    for (int i = 0; i < nm->count; i++) {
        Notification *n = &nm->notifs[i];
        if (!n->alive || n->alpha <= 0) continue;

        int x = n->x, y = n->y;
        uint32_t accent = NOTIF_ACCENT[n->type & 3];

        NvRect shadow = { x + 3, y + 3, NOTIF_W, NOTIF_H };
        draw_fill_rect_alpha(surface, &shadow,
                             (0x00000000 | (uint32_t)(n->alpha / 4)));

        NvRect bg = { x, y, NOTIF_W, NOTIF_H };
        draw_fill_rect_alpha(surface, &bg,
                             (NOTIF_BG & 0xFFFFFF00) |
                             (uint32_t)((NOTIF_BG & 0xFF) * n->alpha / 255));
        draw_rect(surface, &bg, NOTIF_BORDER);

        NvRect bar = { x, y, 4, NOTIF_H };
        draw_fill_rect(surface, &bar, accent);

        int tx = x + 4 + NOTIF_PADDING;
        font_draw_string(surface, tx, y + 12,
                         n->title, NOTIF_FG, 13);
        font_draw_string_clip(surface, tx, y + 32,
                              n->body, NOTIF_DIM, 12,
                              NOTIF_W - 4 - NOTIF_PADDING * 2 - CLOSE_BTN_SIZE - 4);

        int cx = x + NOTIF_W - CLOSE_BTN_SIZE - NOTIF_PADDING / 2;
        int cy = y + NOTIF_PADDING / 2;
        font_draw_string(surface, cx, cy, "x", CLOSE_FG, 12);

        int bar_y = y + NOTIF_H - 3;
        float progress = 1.0f - (float)n->age_ms / NOTIF_DURATION_MS;
        if (progress < 0.0f) progress = 0.0f;
        int bar_w = (int)(NOTIF_W * progress);
        NvRect progress_bar = { x, bar_y, bar_w, 3 };
        draw_fill_rect(surface, &progress_bar, accent);
    }
}

void notif_mouse_down(NotifManager *nm, int x, int y) {
    if (!nm) return;
    for (int i = 0; i < nm->count; i++) {
        Notification *n = &nm->notifs[i];
        if (!n->alive) continue;
        int cx = n->x + NOTIF_W - CLOSE_BTN_SIZE - NOTIF_PADDING / 2;
        int cy = n->y + NOTIF_PADDING / 2;
        if (x >= cx - 2 && x <= cx + CLOSE_BTN_SIZE + 2 &&
            y >= cy - 2 && y <= cy + CLOSE_BTN_SIZE + 2) {
            n->closing = 1;
            return;
        }
    }
}

int notif_has_active(NotifManager *nm) {
    if (!nm) return 0;
    for (int i = 0; i < nm->count; i++)
        if (nm->notifs[i].alive) return 1;
    return 0;
}