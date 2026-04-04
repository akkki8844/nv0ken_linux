#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include "../draw/draw.h"
#include "../font/font_render.h"
#include <stdint.h>

#define NOTIF_W           300
#define NOTIF_H           70
#define NOTIF_PADDING     12
#define NOTIF_MARGIN      8
#define NOTIF_DURATION_MS 4000
#define NOTIF_ANIM_MS     300
#define NOTIF_MAX         8

typedef enum {
    NOTIF_INFO,
    NOTIF_SUCCESS,
    NOTIF_WARNING,
    NOTIF_ERROR,
} NotifType;

typedef struct NotifManager NotifManager;

NotifManager *notif_manager_new(int screen_w, int screen_h);
void          notif_manager_free(NotifManager *nm);

void          notif_push(NotifManager *nm, NotifType type,
                         const char *title, const char *body);
void          notif_tick(NotifManager *nm);
void          notif_draw(NotifManager *nm, NvSurface *surface);
void          notif_mouse_down(NotifManager *nm, int x, int y);
int           notif_has_active(NotifManager *nm);

#endif