#include "launcher.h"
#include "../draw/draw.h"
#include "../draw/color.h"
#include "../font/font_render.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define BG_COLOR         0x1E1E1EEE
#define BG_SOLID         0x252526FF
#define BORDER_COLOR     0x3F3F46FF
#define SEARCH_BG        0x333333FF
#define SEARCH_CURSOR    0x007ACCFF
#define SEARCH_FG        0xCCCCCCFF
#define SEARCH_HINT      0x666666FF
#define ITEM_NORMAL      0x00000000
#define ITEM_HOVERED     0x0078D730
#define ITEM_SELECTED    0x007ACCFF
#define ITEM_FG          0xCCCCCCFF
#define ITEM_FG_DIM      0x888888FF
#define SCROLLBAR_BG     0x333333FF
#define SCROLLBAR_FG     0x555555FF
#define SHADOW_COLOR     0x00000088
#define HEADER_FG        0x888888FF
#define CATEGORY_LINE    0x333333FF

static Launcher *g_launcher = NULL;

struct Launcher {
    Compositor    *comp;
    WindowManager *wm;

    int            x, y, w, h;
    int            visible;

    LauncherItem   all_items[LAUNCHER_MAX_ITEMS];
    int            all_count;

    LauncherItem  *filtered[LAUNCHER_MAX_ITEMS];
    int            filtered_count;

    char           search[128];
    int            search_len;
    int            search_cursor;

    int            hovered_item;
    int            selected_item;
    int            scroll_offset;

    int            needs_redraw;
};

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

static int str_icontains(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return 1;
    size_t hl = strlen(haystack), nl = strlen(needle);
    for (size_t i = 0; i + nl <= hl; i++) {
        int ok = 1;
        for (size_t j = 0; j < nl; j++) {
            if (tolower((unsigned char)haystack[i+j]) !=
                tolower((unsigned char)needle[j])) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

static void rebuild_filtered(Launcher *l) {
    l->filtered_count = 0;
    for (int i = 0; i < l->all_count; i++) {
        if (l->search_len == 0 ||
            str_icontains(l->all_items[i].name, l->search) ||
            str_icontains(l->all_items[i].exec, l->search) ||
            str_icontains(l->all_items[i].category, l->search)) {
            l->filtered[l->filtered_count++] = &l->all_items[i];
        }
    }
    l->hovered_item  = l->filtered_count > 0 ? 0 : -1;
    l->selected_item = -1;
    l->scroll_offset = 0;
    l->needs_redraw  = 1;
}

static void clamp_scroll(Launcher *l) {
    int max_scroll = l->filtered_count - LAUNCHER_MAX_VISIBLE;
    if (max_scroll < 0) max_scroll = 0;
    if (l->scroll_offset > max_scroll) l->scroll_offset = max_scroll;
    if (l->scroll_offset < 0) l->scroll_offset = 0;
}

static void ensure_visible(Launcher *l, int idx) {
    if (idx < l->scroll_offset)
        l->scroll_offset = idx;
    else if (idx >= l->scroll_offset + LAUNCHER_MAX_VISIBLE)
        l->scroll_offset = idx - LAUNCHER_MAX_VISIBLE + 1;
    clamp_scroll(l);
}

static void launch_selected(Launcher *l, int idx) {
    if (idx < 0 || idx >= l->filtered_count) return;
    LauncherItem *item = l->filtered[idx];
    if (item->exec[0]) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s &", item->exec);
        nv_exec(cmd);
    }
    launcher_hide(l);
}

static void draw_icon(NvSurface *surface, int x, int y,
                      uint32_t color, int size) {
    NvRect r = { x, y, size, size };
    draw_fill_rect(surface, &r, color);

    int cx = x + size / 2, cy = y + size / 2;
    NvRect dot = { cx - 3, cy - 3, 6, 6 };
    uint32_t lighter = color_lighten(color, 60);
    draw_fill_rect(surface, &dot, lighter);

    NvRect top  = { x + 2, y + 2, size - 4, 4 };
    draw_fill_rect(surface, &top, lighter);
}

Launcher *launcher_new(Compositor *comp, WindowManager *wm) {
    Launcher *l = xmalloc(sizeof(Launcher));
    if (!l) return NULL;

    l->comp   = comp;
    l->wm     = wm;
    l->w      = LAUNCHER_W;
    l->visible = 0;
    l->hovered_item  = -1;
    l->selected_item = -1;
    l->needs_redraw  = 1;

    launcher_load_defaults(l);
    rebuild_filtered(l);
    return l;
}

void launcher_free(Launcher *l) {
    if (!l) return;
    if (g_launcher == l) g_launcher = NULL;
    free(l);
}

void launcher_add_item(Launcher *l, const char *name,
                       const char *exec, const char *category,
                       uint32_t icon_color) {
    if (!l || l->all_count >= LAUNCHER_MAX_ITEMS) return;
    LauncherItem *item = &l->all_items[l->all_count++];
    strncpy(item->name,     name,     sizeof(item->name)     - 1);
    strncpy(item->exec,     exec,     sizeof(item->exec)     - 1);
    strncpy(item->category, category, sizeof(item->category) - 1);
    item->icon_color = icon_color;
    item->hovered    = 0;
}

void launcher_load_defaults(Launcher *l) {
    if (!l) return;
    launcher_add_item(l, "Terminal",    "/bin/terminal",     "System",  0x2196F3FF);
    launcher_add_item(l, "File Manager","/bin/file_manager", "System",  0xFFC107FF);
    launcher_add_item(l, "Text Editor", "/bin/text_editor",  "Utility", 0x4CAF50FF);
    launcher_add_item(l, "Browser",     "/bin/browser",      "Internet",0x03A9F4FF);
    launcher_add_item(l, "Calculator",  "/bin/calculator",   "Utility", 0x9C27B0FF);
    launcher_add_item(l, "Image Viewer","/bin/image_viewer", "Media",   0xFF5722FF);
}

void launcher_show(Launcher *l, int x, int y) {
    if (!l) return;

    int visible_items = l->filtered_count < LAUNCHER_MAX_VISIBLE
                        ? l->filtered_count : LAUNCHER_MAX_VISIBLE;
    l->h = LAUNCHER_SEARCH_H + LAUNCHER_PADDING +
           visible_items * LAUNCHER_ITEM_H + LAUNCHER_PADDING;
    if (l->h < 100) l->h = 100;

    l->x = x;
    l->y = y - l->h;

    l->visible     = 1;
    l->needs_redraw = 1;
    l->search[0]   = '\0';
    l->search_len  = 0;
    l->search_cursor = 0;
    rebuild_filtered(l);
}

void launcher_hide(Launcher *l) {
    if (!l) return;
    l->visible = 0;
    l->needs_redraw = 1;
    l->search[0] = '\0';
    l->search_len = 0;
}

void launcher_toggle(Compositor *comp, WindowManager *wm, int x, int y) {
    if (!g_launcher) {
        g_launcher = launcher_new(comp, wm);
    }
    if (g_launcher->visible)
        launcher_hide(g_launcher);
    else
        launcher_show(g_launcher, x, y);
}

int launcher_visible(Launcher *l) {
    return l ? l->visible : 0;
}

void launcher_draw(Launcher *l, NvSurface *surface) {
    if (!l || !l->visible || !surface) return;

    NvRect shadow = { l->x + 4, l->y + 4, l->w, l->h };
    draw_fill_rect_alpha(surface, &shadow, SHADOW_COLOR);

    NvRect bg = { l->x, l->y, l->w, l->h };
    draw_fill_rect(surface, &bg, BG_SOLID);
    draw_rect(surface, &bg, BORDER_COLOR);

    int sy  = l->y + LAUNCHER_PADDING;
    int sw  = l->w - LAUNCHER_PADDING * 2;
    NvRect search_bg = { l->x + LAUNCHER_PADDING, sy,
                          sw, LAUNCHER_SEARCH_H };
    draw_fill_rect(surface, &search_bg, SEARCH_BG);
    draw_rect(surface, &search_bg, SEARCH_CURSOR);

    font_draw_string(surface,
                     l->x + LAUNCHER_PADDING + 8, sy + 11,
                     l->search_len ? l->search : "",
                     SEARCH_FG, 13);

    if (!l->search_len) {
        font_draw_string(surface,
                         l->x + LAUNCHER_PADDING + 8, sy + 11,
                         "Search applications...",
                         SEARCH_HINT, 13);
    } else {
        int cx = l->x + LAUNCHER_PADDING + 8 + l->search_cursor * 7;
        NvRect caret = { cx, sy + 8, 1, 18 };
        draw_fill_rect(surface, &caret, SEARCH_CURSOR);
    }

    int list_y = sy + LAUNCHER_SEARCH_H + LAUNCHER_PADDING;
    int visible = l->filtered_count < LAUNCHER_MAX_VISIBLE
                  ? l->filtered_count : LAUNCHER_MAX_VISIBLE;

    for (int i = 0; i < visible; i++) {
        int idx = i + l->scroll_offset;
        if (idx >= l->filtered_count) break;

        LauncherItem *item = l->filtered[idx];
        int iy = list_y + i * LAUNCHER_ITEM_H;

        if (idx == l->hovered_item) {
            NvRect hi = { l->x + 2, iy, l->w - 4, LAUNCHER_ITEM_H };
            draw_fill_rect_alpha(surface, &hi, ITEM_HOVERED);
            NvRect left_bar = { l->x + 2, iy + 4, 3, LAUNCHER_ITEM_H - 8 };
            draw_fill_rect(surface, &left_bar, SEARCH_CURSOR);
        }

        draw_icon(surface,
                  l->x + LAUNCHER_PADDING,
                  iy + (LAUNCHER_ITEM_H - LAUNCHER_ICON_SIZE) / 2,
                  item->icon_color, LAUNCHER_ICON_SIZE);

        int tx = l->x + LAUNCHER_PADDING + LAUNCHER_ICON_SIZE + 10;
        font_draw_string(surface, tx, iy + 8,
                         item->name, ITEM_FG, 14);
        font_draw_string(surface, tx, iy + 24,
                         item->category, ITEM_FG_DIM, 11);
    }

    if (l->filtered_count == 0 && l->search_len > 0) {
        font_draw_string(surface,
                         l->x + LAUNCHER_PADDING + 8,
                         list_y + 14,
                         "No results", SEARCH_HINT, 13);
    }

    if (l->filtered_count > LAUNCHER_MAX_VISIBLE) {
        int sb_x   = l->x + l->w - 6;
        int sb_h   = visible * LAUNCHER_ITEM_H;
        int thumb_h = (sb_h * LAUNCHER_MAX_VISIBLE) / l->filtered_count;
        int thumb_y = list_y + (sb_h * l->scroll_offset) / l->filtered_count;
        if (thumb_h < 12) thumb_h = 12;

        NvRect track = { sb_x, list_y, 4, sb_h };
        draw_fill_rect(surface, &track, SCROLLBAR_BG);

        NvRect thumb = { sb_x, thumb_y, 4, thumb_h };
        draw_fill_rect(surface, &thumb, SCROLLBAR_FG);
    }

    l->needs_redraw = 0;
}

void launcher_mouse_down(Launcher *l, int x, int y) {
    if (!l || !l->visible) return;

    if (x < l->x || x >= l->x + l->w ||
        y < l->y || y >= l->y + l->h) {
        launcher_hide(l);
        return;
    }

    int list_y = l->y + LAUNCHER_PADDING + LAUNCHER_SEARCH_H + LAUNCHER_PADDING;
    if (y >= list_y) {
        int idx = (y - list_y) / LAUNCHER_ITEM_H + l->scroll_offset;
        if (idx >= 0 && idx < l->filtered_count)
            launch_selected(l, idx);
    }
}

void launcher_mouse_move(Launcher *l, int x, int y) {
    if (!l || !l->visible) return;

    int prev = l->hovered_item;
    l->hovered_item = -1;

    int list_y = l->y + LAUNCHER_PADDING + LAUNCHER_SEARCH_H + LAUNCHER_PADDING;
    if (x >= l->x && x < l->x + l->w && y >= list_y) {
        int idx = (y - list_y) / LAUNCHER_ITEM_H + l->scroll_offset;
        if (idx >= 0 && idx < l->filtered_count)
            l->hovered_item = idx;
    }

    if (l->hovered_item != prev) l->needs_redraw = 1;
}

int launcher_key_down(Launcher *l, int key, uint32_t codepoint) {
    if (!l || !l->visible) return 0;

    switch (key) {
        case NV_KEY_ESCAPE:
            launcher_hide(l);
            return 1;

        case NV_KEY_RETURN:
            launch_selected(l, l->hovered_item >= 0 ? l->hovered_item : 0);
            return 1;

        case NV_KEY_UP:
            if (l->hovered_item > 0) {
                l->hovered_item--;
                ensure_visible(l, l->hovered_item);
                l->needs_redraw = 1;
            }
            return 1;

        case NV_KEY_DOWN:
            if (l->hovered_item < l->filtered_count - 1) {
                l->hovered_item++;
                ensure_visible(l, l->hovered_item);
                l->needs_redraw = 1;
            }
            return 1;

        case NV_KEY_BACKSPACE:
            if (l->search_len > 0) {
                l->search[--l->search_len] = '\0';
                l->search_cursor = l->search_len;
                rebuild_filtered(l);
            }
            return 1;

        case NV_KEY_PAGE_UP:
            l->scroll_offset -= LAUNCHER_MAX_VISIBLE;
            clamp_scroll(l);
            l->needs_redraw = 1;
            return 1;

        case NV_KEY_PAGE_DOWN:
            l->scroll_offset += LAUNCHER_MAX_VISIBLE;
            clamp_scroll(l);
            l->needs_redraw = 1;
            return 1;

        default:
            if (codepoint >= 32 && codepoint < 127 &&
                l->search_len < (int)sizeof(l->search) - 1) {
                l->search[l->search_len++] = (char)codepoint;
                l->search[l->search_len]   = '\0';
                l->search_cursor = l->search_len;
                rebuild_filtered(l);
            }
            return 1;
    }
}