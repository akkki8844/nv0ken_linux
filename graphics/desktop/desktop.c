#include "desktop.h"
#include "wallpaper.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include "../draw/image.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define DOUBLE_CLICK_MS     400
#define SELECTION_COLOR     0x3078D7FF
#define SELECTION_BG        0x1A78D730
#define ICON_HOVER_ALPHA    0x20
#define CONTEXT_ITEM_H      28
#define CONTEXT_W           180
#define CONTEXT_PADDING_X   12
#define SHADOW_ALPHA        0x44

struct Desktop {
    Compositor    *comp;
    WindowManager *wm;

    int            x, y, w, h;

    uint32_t      *wallpaper_pixels;
    int            wallpaper_w;
    int            wallpaper_h;
    char           wallpaper_path[256];

    DesktopIcon    icons[DESKTOP_MAX_ICONS];
    int            icon_count;

    int            selected_icon;
    int            drag_active;
    int            drag_offset_x;
    int            drag_offset_y;

    int            last_click_x;
    int            last_click_y;
    int            last_click_time;
    int            last_click_icon;

    ContextMenu    ctx_menu;
    int            ctx_target_icon;

    int            rubber_band;
    int            rb_start_x;
    int            rb_start_y;
    int            rb_end_x;
    int            rb_end_y;

    int            needs_redraw;
};

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

static int icon_hit(DesktopIcon *ic, int x, int y) {
    return x >= ic->x && x < ic->x + DESKTOP_ICON_SIZE &&
           y >= ic->y && y < ic->y + DESKTOP_ICON_SIZE + DESKTOP_ICON_LABEL_H;
}

static int get_tick_ms(void) {
    static long long epoch = 0;
    long long t;
    sys_time(&t);
    if (!epoch) epoch = t;
    return (int)((t - epoch) * 1000);
}

static void arrange_icons(Desktop *d) {
    int col = 0, row = 0;
    for (int i = 0; i < d->icon_count; i++) {
        d->icons[i].x = d->x + DESKTOP_ICON_GRID_X +
                         col * DESKTOP_ICON_SPACING_X;
        d->icons[i].y = d->y + DESKTOP_ICON_GRID_Y +
                         row * DESKTOP_ICON_SPACING_Y;
        row++;
        if (row >= DESKTOP_ICONS_PER_COL) { row = 0; col++; }
    }
}

/* -----------------------------------------------------------------------
 * Icon drawing
 * --------------------------------------------------------------------- */

static void draw_icon_shadow(NvSurface *surface, int x, int y) {
    for (int dy = 2; dy <= 5; dy++) {
        for (int dx = 2; dx <= 5; dx++) {
            NvRect r = { x + dx, y + dy, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE };
            draw_fill_rect_alpha(surface, &r, 0x00000000 | (SHADOW_ALPHA >> dy));
        }
    }
}

static void draw_icon(NvSurface *surface, DesktopIcon *ic) {
    draw_icon_shadow(surface, ic->x, ic->y);

    NvRect icon_rect = { ic->x, ic->y, DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE };

    if (ic->icon_pixels) {
        draw_image_scaled(surface, ic->icon_pixels,
                          ic->icon_w, ic->icon_h,
                          ic->x, ic->y,
                          DESKTOP_ICON_SIZE, DESKTOP_ICON_SIZE);
    } else {
        uint32_t bg = ic->type == ICON_FOLDER ? 0xFFC107FF :
                      ic->type == ICON_APP    ? 0x2196F3FF :
                      ic->type == ICON_LINK   ? 0x9C27B0FF :
                                                0x607D8BFF;
        draw_fill_rect(surface, &icon_rect, bg);

        const char *glyph = ic->type == ICON_FOLDER ? "D" :
                            ic->type == ICON_APP    ? "A" :
                            ic->type == ICON_LINK   ? "L" : "F";
        font_draw_char(surface,
                       ic->x + DESKTOP_ICON_SIZE / 2 - 8,
                       ic->y + DESKTOP_ICON_SIZE / 2 - 10,
                       glyph[0], 0xFFFFFFFF, 20, 1);
    }

    if (ic->selected) {
        draw_fill_rect_alpha(surface, &icon_rect, SELECTION_BG);
        draw_rect(surface, &icon_rect, SELECTION_COLOR);
    } else if (ic->hovered) {
        NvRect hover = { ic->x - 2, ic->y - 2,
                         DESKTOP_ICON_SIZE + 4, DESKTOP_ICON_SIZE + 4 };
        draw_fill_rect_alpha(surface, &hover,
                             0xFFFFFF00 | ICON_HOVER_ALPHA);
    }

    int label_x = ic->x + DESKTOP_ICON_SIZE / 2;
    int label_y = ic->y + DESKTOP_ICON_SIZE + 4;
    int label_w = (int)strlen(ic->label) * 7;

    if (label_w > DESKTOP_ICON_SIZE + 16) label_w = DESKTOP_ICON_SIZE + 16;
    int lx = label_x - label_w / 2;

    if (ic->selected) {
        NvRect label_bg = { lx - 2, label_y - 2, label_w + 4, 16 };
        draw_fill_rect(surface, &label_bg, SELECTION_COLOR);
        font_draw_string_clip(surface, lx, label_y, ic->label,
                              0xFFFFFFFF, 12, label_w);
    } else {
        NvRect label_shadow = { lx + 1, label_y + 1, label_w, 14 };
        draw_fill_rect_alpha(surface, &label_shadow, 0x00000066);
        font_draw_string_clip(surface, lx, label_y, ic->label,
                              0xFFFFFFFF, 12, label_w);
    }
}

/* -----------------------------------------------------------------------
 * Context menu drawing
 * --------------------------------------------------------------------- */

static void draw_context_menu(Desktop *d, NvSurface *surface) {
    ContextMenu *m = &d->ctx_menu;
    if (!m->visible) return;

    int total_h = m->item_count * CONTEXT_ITEM_H + 8;

    NvRect shadow = { m->x + 3, m->y + 3, m->w, total_h };
    draw_fill_rect_alpha(surface, &shadow, 0x00000055);

    NvRect bg = { m->x, m->y, m->w, total_h };
    draw_fill_rect(surface, &bg, 0xF5F5F5FF);
    draw_rect(surface, &bg, 0xCCCCCCFF);

    for (int i = 0; i < m->item_count; i++) {
        int iy = m->y + 4 + i * CONTEXT_ITEM_H;

        if (i == m->hovered_item) {
            NvRect hi = { m->x + 1, iy, m->w - 2, CONTEXT_ITEM_H };
            draw_fill_rect(surface, &hi, 0x0078D7FF);
            font_draw_string(surface,
                             m->x + CONTEXT_PADDING_X, iy + 7,
                             m->items[i], 0xFFFFFFFF, 13);
        } else {
            if (m->items[i][0] == '-') {
                NvRect sep = { m->x + 4, iy + CONTEXT_ITEM_H / 2,
                               m->w - 8, 1 };
                draw_fill_rect(surface, &sep, 0xCCCCCCFF);
            } else {
                font_draw_string(surface,
                                 m->x + CONTEXT_PADDING_X, iy + 7,
                                 m->items[i], 0x111111FF, 13);
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Rubber band selection drawing
 * --------------------------------------------------------------------- */

static void draw_rubber_band(Desktop *d, NvSurface *surface) {
    if (!d->rubber_band) return;

    int x1 = d->rb_start_x < d->rb_end_x ? d->rb_start_x : d->rb_end_x;
    int y1 = d->rb_start_y < d->rb_end_y ? d->rb_start_y : d->rb_end_y;
    int x2 = d->rb_start_x > d->rb_end_x ? d->rb_start_x : d->rb_end_x;
    int y2 = d->rb_start_y > d->rb_end_y ? d->rb_start_y : d->rb_end_y;

    NvRect r = { x1, y1, x2 - x1, y2 - y1 };
    draw_fill_rect_alpha(surface, &r, 0x0078D720);
    draw_rect(surface, &r, 0x0078D7CC);
}

/* -----------------------------------------------------------------------
 * Context menu setup
 * --------------------------------------------------------------------- */

static void show_context_menu_desktop(Desktop *d, int x, int y) {
    ContextMenu *m = &d->ctx_menu;
    m->x = x; m->y = y; m->w = CONTEXT_W;
    m->hovered_item = -1;
    m->visible = 1;
    d->ctx_target_icon = -1;

    strcpy(m->items[0],   "New Folder");
    strcpy(m->actions[0], "new_folder");
    strcpy(m->items[1],   "New File");
    strcpy(m->actions[1], "new_file");
    strcpy(m->items[2],   "-");
    strcpy(m->actions[2], "");
    strcpy(m->items[3],   "Change Wallpaper");
    strcpy(m->actions[3], "wallpaper");
    strcpy(m->items[4],   "-");
    strcpy(m->actions[4], "");
    strcpy(m->items[5],   "Refresh");
    strcpy(m->actions[5], "refresh");
    m->item_count = 6;

    int total_h = m->item_count * CONTEXT_ITEM_H + 8;
    if (m->x + m->w > d->x + d->w)  m->x = d->x + d->w - m->w - 4;
    if (m->y + total_h > d->y + d->h) m->y = d->y + d->h - total_h - 4;
}

static void show_context_menu_icon(Desktop *d, int icon_idx, int x, int y) {
    ContextMenu *m = &d->ctx_menu;
    m->x = x; m->y = y; m->w = CONTEXT_W;
    m->hovered_item = -1;
    m->visible = 1;
    d->ctx_target_icon = icon_idx;

    DesktopIcon *ic = &d->icons[icon_idx];
    int n = 0;

    if (ic->type == ICON_APP || ic->type == ICON_LINK) {
        strcpy(m->items[n],   "Open");
        strcpy(m->actions[n], "open");
        n++;
    }
    if (ic->type == ICON_FOLDER) {
        strcpy(m->items[n],   "Open");
        strcpy(m->actions[n], "open");
        n++;
        strcpy(m->items[n],   "Open in Terminal");
        strcpy(m->actions[n], "open_terminal");
        n++;
    }
    if (ic->type == ICON_FILE) {
        strcpy(m->items[n],   "Open");
        strcpy(m->actions[n], "open");
        n++;
        strcpy(m->items[n],   "Open With...");
        strcpy(m->actions[n], "open_with");
        n++;
    }
    strcpy(m->items[n],   "-");
    strcpy(m->actions[n], "");
    n++;
    strcpy(m->items[n],   "Rename");
    strcpy(m->actions[n], "rename");
    n++;
    strcpy(m->items[n],   "Delete");
    strcpy(m->actions[n], "delete");
    n++;
    strcpy(m->items[n],   "-");
    strcpy(m->actions[n], "");
    n++;
    strcpy(m->items[n],   "Properties");
    strcpy(m->actions[n], "properties");
    n++;
    m->item_count = n;

    int total_h = m->item_count * CONTEXT_ITEM_H + 8;
    if (m->x + m->w > d->x + d->w)   m->x = d->x + d->w - m->w - 4;
    if (m->y + total_h > d->y + d->h) m->y = d->y + d->h - total_h - 4;
}

static void dispatch_context_action(Desktop *d, const char *action) {
    if (strcmp(action, "open") == 0) {
        if (d->ctx_target_icon >= 0) {
            DesktopIcon *ic = &d->icons[d->ctx_target_icon];
            if (ic->exec[0]) {
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "%s &", ic->exec);
                nv_exec(cmd);
            }
        }
    } else if (strcmp(action, "open_terminal") == 0) {
        if (d->ctx_target_icon >= 0) {
            DesktopIcon *ic = &d->icons[d->ctx_target_icon];
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "/bin/terminal --cwd '%s' &", ic->exec);
            nv_exec(cmd);
        }
    } else if (strcmp(action, "rename") == 0) {
        if (d->ctx_target_icon >= 0) {
            DesktopIcon *ic = &d->icons[d->ctx_target_icon];
            char new_name[64];
            strncpy(new_name, ic->label, sizeof(new_name) - 1);
            if (nv_dialog_input("Rename", "New name:", new_name, sizeof(new_name)) == 1)
                if (new_name[0]) strncpy(ic->label, new_name, sizeof(ic->label) - 1);
        }
    } else if (strcmp(action, "delete") == 0) {
        if (d->ctx_target_icon >= 0) {
            DesktopIcon *ic = &d->icons[d->ctx_target_icon];
            char msg[128];
            snprintf(msg, sizeof(msg), "Delete \"%s\"?", ic->label);
            if (nv_dialog_confirm("Delete", msg) == 1) {
                memmove(&d->icons[d->ctx_target_icon],
                        &d->icons[d->ctx_target_icon + 1],
                        sizeof(DesktopIcon) *
                        (d->icon_count - d->ctx_target_icon - 1));
                d->icon_count--;
                arrange_icons(d);
            }
        }
    } else if (strcmp(action, "wallpaper") == 0) {
        char path[256] = {0};
        if (nv_dialog_open_file("Choose Wallpaper", path, sizeof(path),
                                "*.ppm;*.bmp") == 1 && path[0])
            desktop_set_wallpaper(d, path);
    } else if (strcmp(action, "refresh") == 0) {
        desktop_refresh(d);
    } else if (strcmp(action, "new_folder") == 0) {
        desktop_add_icon(d, "New Folder", "/home", "", ICON_FOLDER);
    } else if (strcmp(action, "new_file") == 0) {
        desktop_add_icon(d, "New File", "", "", ICON_FILE);
    } else if (strcmp(action, "properties") == 0) {
        if (d->ctx_target_icon >= 0) {
            DesktopIcon *ic = &d->icons[d->ctx_target_icon];
            char info[256];
            snprintf(info, sizeof(info),
                     "Name: %s\nType: %s\nPath: %s",
                     ic->label,
                     ic->type == ICON_APP    ? "Application" :
                     ic->type == ICON_FOLDER ? "Folder" :
                     ic->type == ICON_LINK   ? "Link" : "File",
                     ic->exec);
            nv_dialog_info("Properties", info);
        }
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

Desktop *desktop_new(Compositor *comp, WindowManager *wm,
                     int x, int y, int w, int h) {
    Desktop *d = xmalloc(sizeof(Desktop));
    if (!d) return NULL;

    d->comp = comp;
    d->wm   = wm;
    d->x = x; d->y = y;
    d->w = w; d->h = h;
    d->selected_icon = -1;
    d->ctx_target_icon = -1;
    d->ctx_menu.visible = 0;
    d->needs_redraw = 1;

    return d;
}

void desktop_free(Desktop *d) {
    if (!d) return;
    free(d->wallpaper_pixels);
    for (int i = 0; i < d->icon_count; i++)
        free(d->icons[i].icon_pixels);
    free(d);
}

void desktop_add_icon(Desktop *d, const char *label,
                      const char *exec, const char *icon_path,
                      IconType type) {
    if (!d || d->icon_count >= DESKTOP_MAX_ICONS) return;

    DesktopIcon *ic = &d->icons[d->icon_count];
    memset(ic, 0, sizeof(DesktopIcon));

    strncpy(ic->label,     label,     sizeof(ic->label)     - 1);
    strncpy(ic->exec,      exec,      sizeof(ic->exec)      - 1);
    strncpy(ic->icon_path, icon_path, sizeof(ic->icon_path) - 1);
    ic->type = type;

    if (icon_path && icon_path[0]) {
        NvImage *img = image_load(icon_path);
        if (img) {
            ic->icon_pixels = img->pixels;
            ic->icon_w      = img->width;
            ic->icon_h      = img->height;
            img->pixels     = NULL;
            image_free(img);
        }
    }

    d->icon_count++;
    arrange_icons(d);
    d->needs_redraw = 1;
}

void desktop_load_icons(Desktop *d) {
    if (!d) return;

    desktop_add_icon(d, "Terminal",    "/bin/terminal",     "/assets/icons/terminal.ppm",  ICON_APP);
    desktop_add_icon(d, "Files",       "/bin/file_manager", "/assets/icons/file.ppm",      ICON_APP);
    desktop_add_icon(d, "Text Editor", "/bin/text_editor",  "/assets/icons/editor.ppm",    ICON_APP);
    desktop_add_icon(d, "Browser",     "/bin/browser",      "/assets/icons/browser.ppm",   ICON_APP);
    desktop_add_icon(d, "Calculator",  "/bin/calculator",   "",                            ICON_APP);
    desktop_add_icon(d, "Home",        "/home",             "/assets/icons/folder.ppm",    ICON_FOLDER);
}

void desktop_set_wallpaper(Desktop *d, const char *path) {
    if (!d || !path) return;
    strncpy(d->wallpaper_path, path, sizeof(d->wallpaper_path) - 1);
    free(d->wallpaper_pixels);
    d->wallpaper_pixels = NULL;

    NvImage *img = image_load(path);
    if (img) {
        d->wallpaper_pixels = img->pixels;
        d->wallpaper_w      = img->width;
        d->wallpaper_h      = img->height;
        img->pixels = NULL;
        image_free(img);
    }
    d->needs_redraw = 1;
}

void desktop_draw(Desktop *d, NvSurface *surface) {
    if (!d || !surface) return;

    if (d->wallpaper_pixels) {
        wallpaper_draw(surface, d->wallpaper_pixels,
                       d->wallpaper_w, d->wallpaper_h,
                       d->x, d->y, d->w, d->h,
                       WALLPAPER_STRETCH);
    } else {
        NvRect bg = { d->x, d->y, d->w, d->h };
        draw_fill_rect(surface, &bg, 0x1A237EFF);

        for (int ty = d->y; ty < d->y + d->h; ty += 2) {
            uint32_t alpha = (uint32_t)(((ty - d->y) * 40) / d->h);
            NvRect line = { d->x, ty, d->w, 2 };
            draw_fill_rect_alpha(surface, &line, alpha);
        }
    }

    for (int i = 0; i < d->icon_count; i++)
        draw_icon(surface, &d->icons[i]);

    draw_rubber_band(d, surface);
    draw_context_menu(d, surface);

    d->needs_redraw = 0;
}

void desktop_mouse_down(Desktop *d, int x, int y, int button) {
    if (!d) return;

    if (d->ctx_menu.visible) {
        ContextMenu *m = &d->ctx_menu;
        int total_h = m->item_count * CONTEXT_ITEM_H + 8;
        if (x >= m->x && x < m->x + m->w &&
            y >= m->y && y < m->y + total_h) {
            int item = (y - m->y - 4) / CONTEXT_ITEM_H;
            if (item >= 0 && item < m->item_count &&
                m->items[item][0] != '-') {
                dispatch_context_action(d, m->actions[item]);
            }
        }
        d->ctx_menu.visible = 0;
        d->needs_redraw = 1;
        return;
    }

    int hit = -1;
    for (int i = d->icon_count - 1; i >= 0; i--) {
        if (icon_hit(&d->icons[i], x, y)) { hit = i; break; }
    }

    if (button == NV_MOUSE_RIGHT) {
        if (hit >= 0) show_context_menu_icon(d, hit, x, y);
        else          show_context_menu_desktop(d, x, y);
        d->needs_redraw = 1;
        return;
    }

    if (hit >= 0) {
        int tick = get_tick_ms();
        int dbl  = (tick - d->last_click_time < DOUBLE_CLICK_MS &&
                    d->last_click_icon == hit);
        d->last_click_time = tick;
        d->last_click_icon = hit;

        if (dbl) {
            desktop_double_click(d, x, y);
            return;
        }

        for (int i = 0; i < d->icon_count; i++) d->icons[i].selected = 0;
        d->icons[hit].selected = 1;
        d->selected_icon = hit;

        d->drag_active   = 1;
        d->drag_offset_x = x - d->icons[hit].x;
        d->drag_offset_y = y - d->icons[hit].y;
    } else {
        for (int i = 0; i < d->icon_count; i++) d->icons[i].selected = 0;
        d->selected_icon = -1;
        d->rubber_band   = 1;
        d->rb_start_x    = x;
        d->rb_start_y    = y;
        d->rb_end_x      = x;
        d->rb_end_y      = y;
    }

    d->needs_redraw = 1;
}

void desktop_mouse_up(Desktop *d, int x, int y, int button) {
    if (!d) return;
    (void)button;

    d->drag_active = 0;

    if (d->rubber_band) {
        d->rubber_band = 0;
        int x1 = d->rb_start_x < d->rb_end_x ? d->rb_start_x : d->rb_end_x;
        int y1 = d->rb_start_y < d->rb_end_y ? d->rb_start_y : d->rb_end_y;
        int x2 = d->rb_start_x > d->rb_end_x ? d->rb_start_x : d->rb_end_x;
        int y2 = d->rb_start_y > d->rb_end_y ? d->rb_start_y : d->rb_end_y;

        for (int i = 0; i < d->icon_count; i++) {
            DesktopIcon *ic = &d->icons[i];
            int cx = ic->x + DESKTOP_ICON_SIZE / 2;
            int cy = ic->y + DESKTOP_ICON_SIZE / 2;
            ic->selected = (cx >= x1 && cx <= x2 && cy >= y1 && cy <= y2);
        }
    }

    d->needs_redraw = 1;
}

void desktop_mouse_move(Desktop *d, int x, int y) {
    if (!d) return;

    if (d->drag_active && d->selected_icon >= 0) {
        DesktopIcon *ic = &d->icons[d->selected_icon];
        ic->x = x - d->drag_offset_x;
        ic->y = y - d->drag_offset_y;
        if (ic->x < d->x) ic->x = d->x;
        if (ic->y < d->y) ic->y = d->y;
        if (ic->x + DESKTOP_ICON_SIZE > d->x + d->w)
            ic->x = d->x + d->w - DESKTOP_ICON_SIZE;
        if (ic->y + DESKTOP_ICON_SIZE > d->y + d->h)
            ic->y = d->y + d->h - DESKTOP_ICON_SIZE;
        d->needs_redraw = 1;
        return;
    }

    if (d->rubber_band) {
        d->rb_end_x = x;
        d->rb_end_y = y;
        d->needs_redraw = 1;
        return;
    }

    if (d->ctx_menu.visible) {
        ContextMenu *m = &d->ctx_menu;
        int prev = m->hovered_item;
        int item = (y - m->y - 4) / CONTEXT_ITEM_H;
        m->hovered_item = (x >= m->x && x < m->x + m->w &&
                           item >= 0 && item < m->item_count) ? item : -1;
        if (m->hovered_item != prev) d->needs_redraw = 1;
        return;
    }

    for (int i = 0; i < d->icon_count; i++) {
        int was = d->icons[i].hovered;
        d->icons[i].hovered = icon_hit(&d->icons[i], x, y);
        if (d->icons[i].hovered != was) d->needs_redraw = 1;
    }
}

void desktop_double_click(Desktop *d, int x, int y) {
    if (!d) return;
    for (int i = 0; i < d->icon_count; i++) {
        if (!icon_hit(&d->icons[i], x, y)) continue;
        DesktopIcon *ic = &d->icons[i];
        if (ic->exec[0]) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "%s &", ic->exec);
            nv_exec(cmd);
        }
        return;
    }
}

void desktop_key_down(Desktop *d, int key, int mods) {
    if (!d) return;

    if (d->ctx_menu.visible && key == NV_KEY_ESCAPE) {
        d->ctx_menu.visible = 0;
        d->needs_redraw = 1;
        return;
    }

    if (key == NV_KEY_DELETE && d->selected_icon >= 0) {
        DesktopIcon *ic = &d->icons[d->selected_icon];
        char msg[128];
        snprintf(msg, sizeof(msg), "Delete \"%s\"?", ic->label);
        if (nv_dialog_confirm("Delete", msg) == 1) {
            memmove(&d->icons[d->selected_icon],
                    &d->icons[d->selected_icon + 1],
                    sizeof(DesktopIcon) *
                    (d->icon_count - d->selected_icon - 1));
            d->icon_count--;
            d->selected_icon = -1;
            arrange_icons(d);
        }
        d->needs_redraw = 1;
        return;
    }

    if (key == NV_KEY_F5) {
        desktop_refresh(d);
        return;
    }

    if ((mods & NV_MOD_CTRL) && key == 'a') {
        for (int i = 0; i < d->icon_count; i++)
            d->icons[i].selected = 1;
        d->needs_redraw = 1;
        return;
    }

    if (key == NV_KEY_RETURN && d->selected_icon >= 0) {
        DesktopIcon *ic = &d->icons[d->selected_icon];
        if (ic->exec[0]) {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "%s &", ic->exec);
            nv_exec(cmd);
        }
    }
}

void desktop_refresh(Desktop *d) {
    if (!d) return;
    for (int i = 0; i < d->icon_count; i++)
        free(d->icons[i].icon_pixels);
    d->icon_count = 0;
    desktop_load_icons(d);
    d->needs_redraw = 1;
}