#include "dir_view.h"
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../userland/libc/include/stdlib.h"
#include "../../../userland/libc/include/string.h"
#include "../../../userland/libc/include/stdio.h"
#include "../../../userland/libc/include/unistd.h"
#include "../../../userland/libc/include/sys/stat.h"

#define ITEM_H           36
#define ICON_W           28
#define ICON_H           28
#define ICON_PAD         6
#define TEXT_PAD_X       40
#define TEXT_PAD_Y       10
#define DETAIL_PAD_Y     22
#define SCROLLBAR_W      10
#define INITIAL_CAP      64
#define MAX_PATH_LEN     1024
#define MAX_NAME_LEN     256

#define COL_BG           NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF)
#define COL_ROW_ALT      NV_COLOR(0xF7, 0xF7, 0xF7, 0xFF)
#define COL_SELECTED     NV_COLOR(0x00, 0x78, 0xD7, 0xFF)
#define COL_SELECTED_FG  NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF)
#define COL_TEXT         NV_COLOR(0x11, 0x11, 0x11, 0xFF)
#define COL_DETAIL       NV_COLOR(0x77, 0x77, 0x77, 0xFF)
#define COL_BORDER       NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_SCROLLBAR_BG NV_COLOR(0xF0, 0xF0, 0xF0, 0xFF)
#define COL_SCROLLBAR_FG NV_COLOR(0xBB, 0xBB, 0xBB, 0xFF)
#define COL_ICON_DIR     NV_COLOR(0xFF, 0xC1, 0x07, 0xFF)
#define COL_ICON_FILE    NV_COLOR(0x60, 0x95, 0xEB, 0xFF)
#define COL_ICON_EXEC    NV_COLOR(0x28, 0xA7, 0x45, 0xFF)
#define COL_ICON_LINK    NV_COLOR(0x9B, 0x59, 0xB6, 0xFF)
#define COL_ICON_FG      NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF)

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n);
    if (!q) return NULL;
    return q;
}

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = xmalloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/* -----------------------------------------------------------------------
 * DirEntry helpers
 * --------------------------------------------------------------------- */

static void entry_free(DirEntry *e) {
    if (!e) return;
    free(e->name);
    free(e->path);
    free(e->size_str);
    free(e->date_str);
    free(e->ext);
}

static const char *get_ext(const char *name) {
    const char *dot = NULL;
    for (const char *p = name; *p; p++)
        if (*p == '.') dot = p;
    return dot ? dot + 1 : "";
}

static void format_size(char *buf, size_t bufsz, long long bytes) {
    if (bytes < 0) {
        snprintf(buf, bufsz, "--");
    } else if (bytes < 1024) {
        snprintf(buf, bufsz, "%lld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, bufsz, "%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, bufsz, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, bufsz, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

static void format_date(char *buf, size_t bufsz, long mtime) {
    snprintf(buf, bufsz, "%ld", mtime);
}

static FileType classify(struct stat *st, const char *name) {
    if (S_ISDIR(st->st_mode))  return FTYPE_DIR;
    if (S_ISLNK(st->st_mode))  return FTYPE_LINK;
    if (st->st_mode & S_IXUSR) return FTYPE_EXEC;

    const char *ext = get_ext(name);
    static const char *image_exts[] = {"png","jpg","jpeg","bmp","gif","ppm","ico",NULL};
    static const char *text_exts[]  = {"txt","md","c","h","asm","sh","conf","json","xml","html","css",NULL};

    for (int i = 0; image_exts[i]; i++)
        if (strcasecmp(ext, image_exts[i]) == 0) return FTYPE_IMAGE;
    for (int i = 0; text_exts[i]; i++)
        if (strcasecmp(ext, text_exts[i]) == 0) return FTYPE_TEXT;

    return FTYPE_FILE;
}

/* -----------------------------------------------------------------------
 * Sorting comparators
 * --------------------------------------------------------------------- */

static DirViewSortMode g_sort_mode  = SORT_NAME;
static int             g_sort_asc   = 1;

static int cmp_entries(const void *a, const void *b) {
    const DirEntry *ea = (const DirEntry *)a;
    const DirEntry *eb = (const DirEntry *)b;

    if (ea->type == FTYPE_DIR && eb->type != FTYPE_DIR) return -1;
    if (ea->type != FTYPE_DIR && eb->type == FTYPE_DIR) return  1;

    int r = 0;
    switch (g_sort_mode) {
        case SORT_NAME:
            r = strcasecmp(ea->name, eb->name);
            break;
        case SORT_SIZE:
            r = (ea->size_bytes > eb->size_bytes) - (ea->size_bytes < eb->size_bytes);
            break;
        case SORT_DATE:
            r = (ea->mtime > eb->mtime) - (ea->mtime < eb->mtime);
            break;
        case SORT_TYPE:
            r = ea->type - eb->type;
            if (r == 0) r = strcasecmp(ea->ext ? ea->ext : "", eb->ext ? eb->ext : "");
            break;
    }
    return g_sort_asc ? r : -r;
}

/* -----------------------------------------------------------------------
 * DirView lifecycle
 * --------------------------------------------------------------------- */

DirView *dir_view_new(int x, int y, int w, int h) {
    DirView *dv = xmalloc(sizeof(DirView));
    if (!dv) return NULL;
    memset(dv, 0, sizeof(DirView));

    dv->x         = x;
    dv->y         = y;
    dv->w         = w;
    dv->h         = h;
    dv->sort_mode = SORT_NAME;
    dv->sort_asc  = 1;
    dv->selected  = -1;
    dv->scroll_y  = 0;
    dv->show_hidden = 0;

    dv->entries   = NULL;
    dv->count     = 0;
    dv->cap       = 0;
    dv->path      = NULL;

    return dv;
}

void dir_view_free(DirView *dv) {
    if (!dv) return;
    for (int i = 0; i < dv->count; i++) entry_free(&dv->entries[i]);
    free(dv->entries);
    free(dv->path);
    free(dv);
}

/* -----------------------------------------------------------------------
 * Loading
 * --------------------------------------------------------------------- */

int dir_view_load(DirView *dv, const char *path) {
    if (!dv || !path) return -1;

    for (int i = 0; i < dv->count; i++) entry_free(&dv->entries[i]);
    dv->count = 0;

    free(dv->path);
    dv->path = xstrdup(path);
    if (!dv->path) return -1;

    dv->selected = -1;
    dv->scroll_y = 0;

    NvDir *dir = nv_opendir(path);
    if (!dir) return -1;

    const char *entry_name;
    while ((entry_name = nv_readdir(dir)) != NULL) {
        if (strcmp(entry_name, ".") == 0) continue;
        if (!dv->show_hidden && entry_name[0] == '.') {
            if (strcmp(entry_name, "..") != 0) continue;
        }

        if (dv->count >= dv->cap) {
            int nc = dv->cap == 0 ? INITIAL_CAP : dv->cap * 2;
            DirEntry *ne = xrealloc(dv->entries, sizeof(DirEntry) * nc);
            if (!ne) break;
            dv->entries = ne;
            dv->cap     = nc;
        }

        DirEntry *e = &dv->entries[dv->count];
        memset(e, 0, sizeof(DirEntry));

        e->name = xstrdup(entry_name);
        if (!e->name) continue;

        char full_path[MAX_PATH_LEN];
        int  path_len = strlen(path);
        int  name_len = strlen(entry_name);
        if (path_len + name_len + 2 < MAX_PATH_LEN) {
            memcpy(full_path, path, path_len);
            if (path[path_len - 1] != '/') full_path[path_len++] = '/';
            memcpy(full_path + path_len, entry_name, name_len + 1);
        } else {
            full_path[0] = '\0';
        }

        e->path = xstrdup(full_path);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            e->type       = classify(&st, entry_name);
            e->size_bytes = S_ISDIR(st.st_mode) ? -1 : (long long)st.st_size;
            e->mtime      = (long)st.st_mtime;
            e->mode       = st.st_mode;
        } else {
            e->type       = FTYPE_FILE;
            e->size_bytes = -1;
            e->mtime      = 0;
            e->mode       = 0;
        }

        e->ext = xstrdup(get_ext(entry_name));

        char sz_buf[32];
        format_size(sz_buf, sizeof(sz_buf), e->size_bytes);
        e->size_str = xstrdup(sz_buf);

        char dt_buf[32];
        format_date(dt_buf, sizeof(dt_buf), e->mtime);
        e->date_str = xstrdup(dt_buf);

        dv->count++;
    }

    nv_closedir(dir);

    g_sort_mode = dv->sort_mode;
    g_sort_asc  = dv->sort_asc;
    qsort(dv->entries, dv->count, sizeof(DirEntry), cmp_entries);

    return 0;
}

void dir_view_refresh(DirView *dv) {
    if (!dv || !dv->path) return;
    char *saved = xstrdup(dv->path);
    if (!saved) return;
    int saved_sel = dv->selected;
    dir_view_load(dv, saved);
    free(saved);
    if (saved_sel < dv->count) dv->selected = saved_sel;
}

void dir_view_set_sort(DirView *dv, DirViewSortMode mode, int ascending) {
    if (!dv) return;
    dv->sort_mode = mode;
    dv->sort_asc  = ascending;
    g_sort_mode   = mode;
    g_sort_asc    = ascending;
    if (dv->count > 0)
        qsort(dv->entries, dv->count, sizeof(DirEntry), cmp_entries);
}

void dir_view_toggle_hidden(DirView *dv) {
    if (!dv) return;
    dv->show_hidden = !dv->show_hidden;
    dir_view_refresh(dv);
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_icon(NvSurface *surface, DirEntry *e, int x, int y) {
    NvColor fill;
    const char *label = "?";

    switch (e->type) {
        case FTYPE_DIR:
            fill  = COL_ICON_DIR;
            label = "D";
            break;
        case FTYPE_EXEC:
            fill  = COL_ICON_EXEC;
            label = "X";
            break;
        case FTYPE_LINK:
            fill  = COL_ICON_LINK;
            label = "L";
            break;
        case FTYPE_IMAGE:
        case FTYPE_TEXT:
        case FTYPE_FILE:
        default:
            fill  = COL_ICON_FILE;
            label = "F";
            break;
    }

    NvRect icon_rect = { x, y, ICON_W, ICON_H };
    nv_draw_fill_rect(surface, &icon_rect, fill);

    if (e->type == FTYPE_DIR) {
        NvRect tab = { x, y - 4, ICON_W / 2, 5 };
        nv_draw_fill_rect(surface, &tab, fill);
    }

    nv_draw_text(surface,
                 x + (ICON_W - 8) / 2,
                 y + (ICON_H - 14) / 2,
                 label, COL_ICON_FG);
}

void dir_view_draw(DirView *dv, NvSurface *surface) {
    if (!dv || !surface) return;

    NvRect bg = { dv->x, dv->y, dv->w, dv->h };
    nv_draw_fill_rect(surface, &bg, COL_BG);

    int content_w = dv->w - SCROLLBAR_W;
    int visible_h = dv->h;
    int first_row = dv->scroll_y / ITEM_H;
    int last_row  = (dv->scroll_y + visible_h) / ITEM_H + 1;
    if (last_row > dv->count) last_row = dv->count;

    for (int i = first_row; i < last_row; i++) {
        DirEntry *e = &dv->entries[i];
        int row_y = dv->y + i * ITEM_H - dv->scroll_y;

        NvRect row_bg = { dv->x, row_y, content_w, ITEM_H };

        if (i == dv->selected) {
            nv_draw_fill_rect(surface, &row_bg, COL_SELECTED);
        } else if (i % 2 == 1) {
            nv_draw_fill_rect(surface, &row_bg, COL_ROW_ALT);
        }

        draw_icon(surface, e,
                  dv->x + ICON_PAD,
                  row_y + (ITEM_H - ICON_H) / 2);

        NvColor name_col   = (i == dv->selected) ? COL_SELECTED_FG : COL_TEXT;
        NvColor detail_col = (i == dv->selected) ? COL_SELECTED_FG : COL_DETAIL;

        nv_draw_text_clip(surface,
                          dv->x + TEXT_PAD_X,
                          row_y + TEXT_PAD_Y,
                          e->name, name_col,
                          content_w - TEXT_PAD_X - 160);

        if (e->size_str) {
            int sz_x = dv->x + content_w - 150;
            nv_draw_text(surface, sz_x, row_y + TEXT_PAD_Y,
                         e->size_str, detail_col);
        }

        if (e->date_str) {
            int dt_x = dv->x + content_w - 80;
            nv_draw_text(surface, dt_x, row_y + TEXT_PAD_Y,
                         e->date_str, detail_col);
        }

        if (e->ext && e->ext[0] && e->type != FTYPE_DIR) {
            nv_draw_text(surface,
                         dv->x + TEXT_PAD_X,
                         row_y + DETAIL_PAD_Y,
                         e->ext, detail_col);
        }

        NvRect sep = { dv->x, row_y + ITEM_H - 1, content_w, 1 };
        nv_draw_fill_rect(surface, &sep, COL_BORDER);
    }

    /* Scrollbar */
    int total_h = dv->count * ITEM_H;
    if (total_h > visible_h) {
        NvRect sb_bg = { dv->x + content_w, dv->y, SCROLLBAR_W, visible_h };
        nv_draw_fill_rect(surface, &sb_bg, COL_SCROLLBAR_BG);

        int thumb_h = (visible_h * visible_h) / total_h;
        if (thumb_h < 20) thumb_h = 20;
        int thumb_y = dv->y + (dv->scroll_y * (visible_h - thumb_h)) /
                              (total_h - visible_h);

        NvRect thumb = { dv->x + content_w + 2, thumb_y, SCROLLBAR_W - 4, thumb_h };
        nv_draw_fill_rect(surface, &thumb, COL_SCROLLBAR_FG);
    }
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

int dir_view_mouse_down(DirView *dv, int x, int y) {
    if (!dv) return DV_ACTION_NONE;

    if (x < dv->x || x >= dv->x + dv->w) return DV_ACTION_NONE;
    if (y < dv->y || y >= dv->y + dv->h) return DV_ACTION_NONE;

    int row = (y - dv->y + dv->scroll_y) / ITEM_H;
    if (row < 0 || row >= dv->count) {
        dv->selected = -1;
        return DV_ACTION_NONE;
    }

    if (row == dv->selected) {
        return DV_ACTION_ACTIVATE;
    }

    dv->selected = row;
    return DV_ACTION_SELECT;
}

int dir_view_scroll(DirView *dv, int delta_y) {
    if (!dv) return 0;
    int total_h = dv->count * ITEM_H;
    int max_scroll = total_h - dv->h;
    if (max_scroll < 0) max_scroll = 0;
    dv->scroll_y += delta_y * ITEM_H;
    if (dv->scroll_y < 0) dv->scroll_y = 0;
    if (dv->scroll_y > max_scroll) dv->scroll_y = max_scroll;
    return 1;
}

int dir_view_key_down(DirView *dv, int key) {
    if (!dv) return DV_ACTION_NONE;

    switch (key) {
        case NV_KEY_UP:
            if (dv->selected > 0) {
                dv->selected--;
                int item_top = dv->selected * ITEM_H;
                if (item_top < dv->scroll_y)
                    dv->scroll_y = item_top;
                return DV_ACTION_SELECT;
            }
            break;

        case NV_KEY_DOWN:
            if (dv->selected < dv->count - 1) {
                dv->selected++;
                int item_bot = (dv->selected + 1) * ITEM_H;
                if (item_bot > dv->scroll_y + dv->h)
                    dv->scroll_y = item_bot - dv->h;
                return DV_ACTION_SELECT;
            }
            break;

        case NV_KEY_RETURN:
            if (dv->selected >= 0 && dv->selected < dv->count)
                return DV_ACTION_ACTIVATE;
            break;

        case NV_KEY_HOME:
            dv->selected = 0;
            dv->scroll_y = 0;
            return DV_ACTION_SELECT;

        case NV_KEY_END:
            dv->selected = dv->count - 1;
            dv->scroll_y = dv->count * ITEM_H - dv->h;
            if (dv->scroll_y < 0) dv->scroll_y = 0;
            return DV_ACTION_SELECT;

        case NV_KEY_PAGE_UP:
            dv->scroll_y -= dv->h;
            if (dv->scroll_y < 0) dv->scroll_y = 0;
            dv->selected = dv->scroll_y / ITEM_H;
            return DV_ACTION_SELECT;

        case NV_KEY_PAGE_DOWN: {
            int max_scroll = dv->count * ITEM_H - dv->h;
            if (max_scroll < 0) max_scroll = 0;
            dv->scroll_y += dv->h;
            if (dv->scroll_y > max_scroll) dv->scroll_y = max_scroll;
            dv->selected = (dv->scroll_y + dv->h - 1) / ITEM_H;
            if (dv->selected >= dv->count) dv->selected = dv->count - 1;
            return DV_ACTION_SELECT;
        }

        case NV_KEY_BACKSPACE:
            return DV_ACTION_GO_UP;

        case NV_KEY_F5:
            dir_view_refresh(dv);
            return DV_ACTION_REFRESH;

        default:
            break;
    }

    return DV_ACTION_NONE;
}

/* -----------------------------------------------------------------------
 * Accessors
 * --------------------------------------------------------------------- */

DirEntry *dir_view_selected_entry(DirView *dv) {
    if (!dv || dv->selected < 0 || dv->selected >= dv->count) return NULL;
    return &dv->entries[dv->selected];
}

const char *dir_view_current_path(DirView *dv) {
    if (!dv) return NULL;
    return dv->path;
}

int dir_view_entry_count(DirView *dv) {
    if (!dv) return 0;
    return dv->count;
}

void dir_view_select_by_name(DirView *dv, const char *name) {
    if (!dv || !name) return;
    for (int i = 0; i < dv->count; i++) {
        if (dv->entries[i].name && strcmp(dv->entries[i].name, name) == 0) {
            dv->selected = i;
            int item_top = i * ITEM_H;
            int item_bot = (i + 1) * ITEM_H;
            if (item_top < dv->scroll_y)
                dv->scroll_y = item_top;
            else if (item_bot > dv->scroll_y + dv->h)
                dv->scroll_y = item_bot - dv->h;
            return;
        }
    }
}