#include "dir_view.h"
#include "ops.h"
#include "../../../libnv0/include/nv0/app.h"
#include "../../../libnv0/include/nv0/window.h"
#include "../../../libnv0/include/nv0/input.h"
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../libnv0/include/nv0/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WINDOW_W        900
#define WINDOW_H        600
#define TOOLBAR_H       40
#define BREADCRUMB_H    28
#define STATUSBAR_H     24
#define SIDEBAR_W       180
#define CONTENT_X       SIDEBAR_W
#define CONTENT_Y       (TOOLBAR_H + BREADCRUMB_H)
#define CONTENT_W       (WINDOW_W - SIDEBAR_W)
#define CONTENT_H       (WINDOW_H - TOOLBAR_H - BREADCRUMB_H - STATUSBAR_H)
#define MAX_PATH_LEN    1024
#define MAX_HISTORY     64
#define MAX_BOOKMARKS   16
#define SIDEBAR_ITEM_H  32

#define COL_BG          NV_COLOR(0xF5, 0xF5, 0xF5, 0xFF)
#define COL_TOOLBAR_BG  NV_COLOR(0xEC, 0xEC, 0xEC, 0xFF)
#define COL_SIDEBAR_BG  NV_COLOR(0xE8, 0xE8, 0xE8, 0xFF)
#define COL_SIDEBAR_SEL NV_COLOR(0x00, 0x78, 0xD7, 0xFF)
#define COL_SIDEBAR_FG  NV_COLOR(0x22, 0x22, 0x22, 0xFF)
#define COL_SIDEBAR_SEL_FG NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF)
#define COL_BORDER      NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_TEXT        NV_COLOR(0x11, 0x11, 0x11, 0xFF)
#define COL_TEXT_MUTED  NV_COLOR(0x77, 0x77, 0x77, 0xFF)
#define COL_BTN_BG      NV_COLOR(0xDD, 0xDD, 0xDD, 0xFF)
#define COL_BTN_FG      NV_COLOR(0x11, 0x11, 0x11, 0xFF)
#define COL_BTN_HOVER   NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF)
#define COL_BREADCRUMB  NV_COLOR(0xF8, 0xF8, 0xF8, 0xFF)
#define COL_STATUS_BG   NV_COLOR(0xEC, 0xEC, 0xEC, 0xFF)
#define COL_DANGER      NV_COLOR(0xFF, 0x45, 0x3A, 0xFF)
#define COL_WHITE       NV_COLOR(0xFF, 0xFF, 0xFF, 0xFF)

typedef struct {
    char label[64];
    char path[MAX_PATH_LEN];
} Bookmark;

typedef struct {
    char path[MAX_PATH_LEN];
} HistoryEntry;

typedef enum {
    CLIP_NONE,
    CLIP_COPY,
    CLIP_CUT,
} ClipMode;

typedef struct {
    NvWindow  *window;
    NvSurface *surface;

    DirView   *view;

    char       history[MAX_HISTORY][MAX_PATH_LEN];
    int        history_count;
    int        history_pos;

    Bookmark   bookmarks[MAX_BOOKMARKS];
    int        bookmark_count;
    int        sidebar_selected;

    char       clipboard_path[MAX_PATH_LEN];
    ClipMode   clip_mode;

    char       status_text[256];
    int        show_hidden;

    OpsProgress *active_op;
} FileManager;

static FileManager g_fm;

/* -----------------------------------------------------------------------
 * Path helpers
 * --------------------------------------------------------------------- */

static void path_parent(const char *path, char *out, size_t out_sz) {
    strncpy(out, path, out_sz - 1);
    out[out_sz - 1] = '\0';
    char *last = strrchr(out, '/');
    if (!last || last == out) {
        strcpy(out, "/");
        return;
    }
    *last = '\0';
}

static void path_join(const char *base, const char *name, char *out, size_t out_sz) {
    int blen = strlen(base);
    if (blen > 0 && base[blen - 1] == '/')
        snprintf(out, out_sz, "%s%s", base, name);
    else
        snprintf(out, out_sz, "%s/%s", base, name);
}

/* -----------------------------------------------------------------------
 * Status
 * --------------------------------------------------------------------- */

static void set_status(FileManager *fm, const char *text) {
    strncpy(fm->status_text, text, sizeof(fm->status_text) - 1);
    fm->status_text[sizeof(fm->status_text) - 1] = '\0';
    nv_surface_invalidate(fm->surface);
}

static void update_status_for_dir(FileManager *fm) {
    int count = dir_view_entry_count(fm->view);
    char buf[256];
    snprintf(buf, sizeof(buf), "%d item%s", count, count == 1 ? "" : "s");
    DirEntry *sel = dir_view_selected_entry(fm->view);
    if (sel) {
        char detail[128];
        if (sel->size_str && sel->type != FTYPE_DIR)
            snprintf(detail, sizeof(detail), "  —  %s  %s",
                     sel->name, sel->size_str);
        else
            snprintf(detail, sizeof(detail), "  —  %s", sel->name);
        strncat(buf, detail, sizeof(buf) - strlen(buf) - 1);
    }
    set_status(fm, buf);
}

/* -----------------------------------------------------------------------
 * Navigation
 * --------------------------------------------------------------------- */

static void nav_push_history(FileManager *fm, const char *path) {
    while (fm->history_count > fm->history_pos + 1) {
        fm->history_count--;
    }
    if (fm->history_count >= MAX_HISTORY) {
        memmove(&fm->history[0], &fm->history[1],
                sizeof(fm->history[0]) * (MAX_HISTORY - 1));
        fm->history_count--;
        if (fm->history_pos > 0) fm->history_pos--;
    }
    strncpy(fm->history[fm->history_count], path, MAX_PATH_LEN - 1);
    fm->history_pos   = fm->history_count;
    fm->history_count++;
}

static void nav_go(FileManager *fm, const char *path) {
    if (!path || !path[0]) return;
    if (dir_view_load(fm->view, path) < 0) {
        char msg[MAX_PATH_LEN + 64];
        snprintf(msg, sizeof(msg), "Cannot open: %s", path);
        set_status(fm, msg);
        return;
    }
    nav_push_history(fm, path);
    update_status_for_dir(fm);
    nv_surface_invalidate(fm->surface);
}

static void nav_back(FileManager *fm) {
    if (fm->history_pos <= 0) return;
    fm->history_pos--;
    dir_view_load(fm->view, fm->history[fm->history_pos]);
    update_status_for_dir(fm);
    nv_surface_invalidate(fm->surface);
}

static void nav_forward(FileManager *fm) {
    if (fm->history_pos >= fm->history_count - 1) return;
    fm->history_pos++;
    dir_view_load(fm->view, fm->history[fm->history_pos]);
    update_status_for_dir(fm);
    nv_surface_invalidate(fm->surface);
}

static void nav_up(FileManager *fm) {
    const char *cur = dir_view_current_path(fm->view);
    if (!cur || strcmp(cur, "/") == 0) return;
    char parent[MAX_PATH_LEN];
    path_parent(cur, parent, sizeof(parent));
    char old_name[MAX_PATH_LEN];
    strncpy(old_name, strrchr(cur, '/') ? strrchr(cur, '/') + 1 : cur,
            MAX_PATH_LEN - 1);
    nav_go(fm, parent);
    dir_view_select_by_name(fm->view, old_name);
}

static void nav_activate(FileManager *fm) {
    DirEntry *e = dir_view_selected_entry(fm->view);
    if (!e) return;

    if (strcmp(e->name, "..") == 0) {
        nav_up(fm);
        return;
    }
    if (strcmp(e->name, ".") == 0) return;

    if (e->type == FTYPE_DIR) {
        char new_path[MAX_PATH_LEN];
        path_join(dir_view_current_path(fm->view), e->name,
                  new_path, sizeof(new_path));
        nav_go(fm, new_path);
    } else if (e->type == FTYPE_EXEC) {
        char cmd[MAX_PATH_LEN + 4];
        snprintf(cmd, sizeof(cmd), "%s &", e->path);
        nv_exec(cmd);
    } else {
        char msg[MAX_PATH_LEN + 32];
        snprintf(msg, sizeof(msg), "Open: %s", e->path);
        set_status(fm, msg);
    }
}

/* -----------------------------------------------------------------------
 * Bookmarks
 * --------------------------------------------------------------------- */

static void bookmarks_init(FileManager *fm) {
    fm->bookmark_count = 0;

    static const struct { const char *label; const char *path; } defaults[] = {
        { "Home",      "/home"     },
        { "Root",      "/"         },
        { "Documents", "/home/docs"},
        { "Downloads", "/home/dl"  },
        { "Desktop",   "/home/desk"},
        { NULL, NULL }
    };

    for (int i = 0; defaults[i].label && fm->bookmark_count < MAX_BOOKMARKS; i++) {
        strncpy(fm->bookmarks[fm->bookmark_count].label, defaults[i].label, 63);
        strncpy(fm->bookmarks[fm->bookmark_count].path,  defaults[i].path, MAX_PATH_LEN - 1);
        fm->bookmark_count++;
    }
    fm->sidebar_selected = 0;
}

static void bookmark_add(FileManager *fm, const char *path) {
    if (fm->bookmark_count >= MAX_BOOKMARKS) return;
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    strncpy(fm->bookmarks[fm->bookmark_count].label, name, 63);
    strncpy(fm->bookmarks[fm->bookmark_count].path, path, MAX_PATH_LEN - 1);
    fm->bookmark_count++;
    nv_surface_invalidate(fm->surface);
}

/* -----------------------------------------------------------------------
 * Clipboard operations
 * --------------------------------------------------------------------- */

static void clip_copy(FileManager *fm) {
    DirEntry *e = dir_view_selected_entry(fm->view);
    if (!e || strcmp(e->name, "..") == 0) return;
    strncpy(fm->clipboard_path, e->path, MAX_PATH_LEN - 1);
    fm->clip_mode = CLIP_COPY;
    char msg[MAX_PATH_LEN + 16];
    snprintf(msg, sizeof(msg), "Copied: %s", e->name);
    set_status(fm, msg);
}

static void clip_cut(FileManager *fm) {
    DirEntry *e = dir_view_selected_entry(fm->view);
    if (!e || strcmp(e->name, "..") == 0) return;
    strncpy(fm->clipboard_path, e->path, MAX_PATH_LEN - 1);
    fm->clip_mode = CLIP_CUT;
    char msg[MAX_PATH_LEN + 16];
    snprintf(msg, sizeof(msg), "Cut: %s", e->name);
    set_status(fm, msg);
}

static void clip_paste(FileManager *fm) {
    if (fm->clip_mode == CLIP_NONE || !fm->clipboard_path[0]) return;
    const char *dest_dir = dir_view_current_path(fm->view);
    if (!dest_dir) return;

    const char *src_name = strrchr(fm->clipboard_path, '/');
    src_name = src_name ? src_name + 1 : fm->clipboard_path;

    char dest_path[MAX_PATH_LEN];
    path_join(dest_dir, src_name, dest_path, sizeof(dest_path));

    if (fm->clip_mode == CLIP_COPY)
        ops_copy(fm->clipboard_path, dest_path, NULL, NULL);
    else
        ops_move(fm->clipboard_path, dest_path, NULL, NULL);

    if (fm->clip_mode == CLIP_CUT) {
        fm->clipboard_path[0] = '\0';
        fm->clip_mode = CLIP_NONE;
    }

    dir_view_refresh(fm->view);
    update_status_for_dir(fm);
    nv_surface_invalidate(fm->surface);
}

/* -----------------------------------------------------------------------
 * File operations via dialog
 * --------------------------------------------------------------------- */

static void action_new_folder(FileManager *fm) {
    char name[256] = {0};
    if (nv_dialog_input("New folder", "Folder name:", name, sizeof(name)) != 1)
        return;
    if (!name[0]) return;

    char new_path[MAX_PATH_LEN];
    path_join(dir_view_current_path(fm->view), name, new_path, sizeof(new_path));
    if (nv_mkdir(new_path, 0755) == 0) {
        dir_view_refresh(fm->view);
        dir_view_select_by_name(fm->view, name);
        update_status_for_dir(fm);
        nv_surface_invalidate(fm->surface);
    } else {
        set_status(fm, "Failed to create folder");
    }
}

static void action_new_file(FileManager *fm) {
    char name[256] = {0};
    if (nv_dialog_input("New file", "File name:", name, sizeof(name)) != 1)
        return;
    if (!name[0]) return;

    char new_path[MAX_PATH_LEN];
    path_join(dir_view_current_path(fm->view), name, new_path, sizeof(new_path));
    int fd = nv_open(new_path, NV_O_CREAT | NV_O_WRONLY, 0644);
    if (fd >= 0) {
        nv_close(fd);
        dir_view_refresh(fm->view);
        dir_view_select_by_name(fm->view, name);
        update_status_for_dir(fm);
        nv_surface_invalidate(fm->surface);
    } else {
        set_status(fm, "Failed to create file");
    }
}

static void action_rename(FileManager *fm) {
    DirEntry *e = dir_view_selected_entry(fm->view);
    if (!e || strcmp(e->name, "..") == 0) return;

    char new_name[256];
    strncpy(new_name, e->name, sizeof(new_name) - 1);
    if (nv_dialog_input("Rename", "New name:", new_name, sizeof(new_name)) != 1)
        return;
    if (!new_name[0] || strcmp(new_name, e->name) == 0) return;

    char dest_path[MAX_PATH_LEN];
    path_join(dir_view_current_path(fm->view), new_name, dest_path, sizeof(dest_path));
    if (ops_move(e->path, dest_path, NULL, NULL) == 0) {
        dir_view_refresh(fm->view);
        dir_view_select_by_name(fm->view, new_name);
        update_status_for_dir(fm);
        nv_surface_invalidate(fm->surface);
    } else {
        set_status(fm, "Rename failed");
    }
}

static void action_delete(FileManager *fm) {
    DirEntry *e = dir_view_selected_entry(fm->view);
    if (!e || strcmp(e->name, "..") == 0) return;

    char msg[MAX_PATH_LEN + 64];
    snprintf(msg, sizeof(msg), "Delete \"%s\"? This cannot be undone.", e->name);
    if (nv_dialog_confirm("Delete", msg) != 1) return;

    if (ops_delete(e->path, NULL, NULL) == 0) {
        dir_view_refresh(fm->view);
        update_status_for_dir(fm);
        nv_surface_invalidate(fm->surface);
    } else {
        set_status(fm, "Delete failed");
    }
}

static void action_properties(FileManager *fm) {
    DirEntry *e = dir_view_selected_entry(fm->view);
    if (!e) return;
    char info[512];
    snprintf(info, sizeof(info),
             "Name: %s\nPath: %s\nSize: %s\nType: %s",
             e->name,
             e->path ? e->path : "-",
             e->size_str ? e->size_str : "-",
             e->type == FTYPE_DIR  ? "Directory" :
             e->type == FTYPE_EXEC ? "Executable" :
             e->type == FTYPE_LINK ? "Symlink" :
             e->type == FTYPE_IMAGE? "Image" :
             e->type == FTYPE_TEXT ? "Text file" : "File");
    nv_dialog_info("Properties", info);
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_toolbar(FileManager *fm) {
    NvRect bg = {0, 0, WINDOW_W, TOOLBAR_H};
    nv_draw_fill_rect(fm->surface, &bg, COL_TOOLBAR_BG);
    NvRect border = {0, TOOLBAR_H - 1, WINDOW_W, 1};
    nv_draw_fill_rect(fm->surface, &border, COL_BORDER);

    int can_back = fm->history_pos > 0;
    int can_fwd  = fm->history_pos < fm->history_count - 1;

    struct { int x; const char *label; int enabled; } btns[] = {
        {  4, "<",    can_back },
        { 36, ">",    can_fwd  },
        { 68, "^",    1        },
        {108, "New folder", 1  },
        {212, "New file",   1  },
        {300, "Rename",     1  },
        {376, "Delete",     1  },
        {448, "Copy",       1  },
        {504, "Cut",        1  },
        {548, "Paste",  fm->clip_mode != CLIP_NONE },
        {604, "Bookmark",   1  },
        {688, "Hidden",     1  },
        { -1, NULL,         0  },
    };

    for (int i = 0; btns[i].label; i++) {
        int bw = (int)strlen(btns[i].label) * 8 + 16;
        NvRect r = {btns[i].x, 6, bw, 28};
        NvColor bg_c = btns[i].enabled ? COL_BTN_BG
                                       : NV_COLOR(0xEE,0xEE,0xEE,0xFF);
        NvColor fg_c = btns[i].enabled ? COL_BTN_FG : COL_TEXT_MUTED;
        nv_draw_fill_rect(fm->surface, &r, bg_c);
        nv_draw_rect(fm->surface, &r, COL_BORDER);
        nv_draw_text(fm->surface, btns[i].x + 8, 13, btns[i].label, fg_c);
    }
}

static void draw_breadcrumb(FileManager *fm) {
    int y = TOOLBAR_H;
    NvRect bg = {0, y, WINDOW_W, BREADCRUMB_H};
    nv_draw_fill_rect(fm->surface, &bg, COL_BREADCRUMB);
    NvRect border = {0, y + BREADCRUMB_H - 1, WINDOW_W, 1};
    nv_draw_fill_rect(fm->surface, &border, COL_BORDER);

    const char *path = dir_view_current_path(fm->view);
    if (!path) return;

    int x = CONTENT_X + 8;
    int by = y + 7;

    char buf[MAX_PATH_LEN];
    strncpy(buf, path, MAX_PATH_LEN - 1);

    char *parts[64];
    int   nparts = 0;
    char *p = buf;

    if (*p == '/') {
        parts[nparts++] = "/";
        p++;
    }

    while (*p && nparts < 63) {
        char *slash = strchr(p, '/');
        if (slash) {
            *slash = '\0';
            if (*p) parts[nparts++] = p;
            p = slash + 1;
        } else {
            if (*p) parts[nparts++] = p;
            break;
        }
    }

    for (int i = 0; i < nparts; i++) {
        nv_draw_text(fm->surface, x, by, parts[i], COL_TEXT);
        x += (int)strlen(parts[i]) * 8 + 2;
        if (i < nparts - 1) {
            nv_draw_text(fm->surface, x, by, " / ", COL_TEXT_MUTED);
            x += 24;
        }
    }
}

static void draw_sidebar(FileManager *fm) {
    NvRect bg = {0, TOOLBAR_H, SIDEBAR_W, WINDOW_H - TOOLBAR_H};
    nv_draw_fill_rect(fm->surface, &bg, COL_SIDEBAR_BG);
    NvRect border = {SIDEBAR_W - 1, TOOLBAR_H, 1, WINDOW_H - TOOLBAR_H};
    nv_draw_fill_rect(fm->surface, &border, COL_BORDER);

    int y = TOOLBAR_H + 8;
    nv_draw_text(fm->surface, 12, y, "Places", COL_TEXT_MUTED);
    y += 20;

    for (int i = 0; i < fm->bookmark_count; i++) {
        NvRect row = {0, y, SIDEBAR_W - 1, SIDEBAR_ITEM_H};
        if (i == fm->sidebar_selected)
            nv_draw_fill_rect(fm->surface, &row, COL_SIDEBAR_SEL);

        NvColor fg = (i == fm->sidebar_selected) ? COL_SIDEBAR_SEL_FG : COL_SIDEBAR_FG;
        nv_draw_text_clip(fm->surface, 12, y + 9,
                          fm->bookmarks[i].label, fg, SIDEBAR_W - 24);
        y += SIDEBAR_ITEM_H;
    }
}

static void draw_statusbar(FileManager *fm) {
    int y = WINDOW_H - STATUSBAR_H;
    NvRect bg = {0, y, WINDOW_W, STATUSBAR_H};
    nv_draw_fill_rect(fm->surface, &bg, COL_STATUS_BG);
    NvRect border = {0, y, WINDOW_W, 1};
    nv_draw_fill_rect(fm->surface, &border, COL_BORDER);
    nv_draw_text(fm->surface, 8, y + 5, fm->status_text, COL_TEXT_MUTED);

    if (fm->clip_mode != CLIP_NONE) {
        const char *clip_label = fm->clip_mode == CLIP_COPY ? "[ copied ]" : "[ cut ]";
        int lw = (int)strlen(clip_label) * 8;
        nv_draw_text(fm->surface, WINDOW_W - lw - 12, y + 5,
                     clip_label, COL_TEXT_MUTED);
    }
}

static void on_paint(NvWindow *win, NvSurface *surface, void *userdata) {
    (void)win;
    FileManager *fm = (FileManager *)userdata;
    fm->surface = surface;

    NvRect bg = {0, 0, WINDOW_W, WINDOW_H};
    nv_draw_fill_rect(surface, &bg, COL_BG);

    draw_toolbar(fm);
    draw_breadcrumb(fm);
    draw_sidebar(fm);
    dir_view_draw(fm->view, surface);
    draw_statusbar(fm);
}

/* -----------------------------------------------------------------------
 * Toolbar hit testing
 * --------------------------------------------------------------------- */

typedef enum {
    TB_NONE,
    TB_BACK, TB_FORWARD, TB_UP,
    TB_NEW_FOLDER, TB_NEW_FILE,
    TB_RENAME, TB_DELETE,
    TB_COPY, TB_CUT, TB_PASTE,
    TB_BOOKMARK, TB_HIDDEN,
} ToolbarBtn;

static ToolbarBtn toolbar_hit(int x, int y) {
    if (y < 6 || y > 34) return TB_NONE;
    if (x >=   4 && x <  32) return TB_BACK;
    if (x >=  36 && x <  64) return TB_FORWARD;
    if (x >=  68 && x <  96) return TB_UP;
    if (x >= 108 && x < 208) return TB_NEW_FOLDER;
    if (x >= 212 && x < 296) return TB_NEW_FILE;
    if (x >= 300 && x < 372) return TB_RENAME;
    if (x >= 376 && x < 444) return TB_DELETE;
    if (x >= 448 && x < 500) return TB_COPY;
    if (x >= 504 && x < 544) return TB_CUT;
    if (x >= 548 && x < 600) return TB_PASTE;
    if (x >= 604 && x < 684) return TB_BOOKMARK;
    if (x >= 688 && x < 772) return TB_HIDDEN;
    return TB_NONE;
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

static void on_mouse_down(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    FileManager *fm = (FileManager *)userdata;

    if (ev->y < TOOLBAR_H) {
        ToolbarBtn btn = toolbar_hit(ev->x, ev->y);
        switch (btn) {
            case TB_BACK:       nav_back(fm);            break;
            case TB_FORWARD:    nav_forward(fm);         break;
            case TB_UP:         nav_up(fm);              break;
            case TB_NEW_FOLDER: action_new_folder(fm);   break;
            case TB_NEW_FILE:   action_new_file(fm);     break;
            case TB_RENAME:     action_rename(fm);       break;
            case TB_DELETE:     action_delete(fm);       break;
            case TB_COPY:       clip_copy(fm);           break;
            case TB_CUT:        clip_cut(fm);            break;
            case TB_PASTE:      clip_paste(fm);          break;
            case TB_BOOKMARK: {
                const char *cur = dir_view_current_path(fm->view);
                if (cur) bookmark_add(fm, cur);
                break;
            }
            case TB_HIDDEN:
                fm->show_hidden = !fm->show_hidden;
                dir_view_toggle_hidden(fm->view);
                update_status_for_dir(fm);
                nv_surface_invalidate(fm->surface);
                break;
            default: break;
        }
        return;
    }

    if (ev->x < SIDEBAR_W) {
        int rel_y = ev->y - TOOLBAR_H - 28;
        int idx   = rel_y / SIDEBAR_ITEM_H;
        if (idx >= 0 && idx < fm->bookmark_count) {
            fm->sidebar_selected = idx;
            nav_go(fm, fm->bookmarks[idx].path);
        }
        return;
    }

    int action = dir_view_mouse_down(fm->view, ev->x, ev->y);
    if (action == DV_ACTION_SELECT) {
        update_status_for_dir(fm);
        nv_surface_invalidate(fm->surface);
    } else if (action == DV_ACTION_ACTIVATE) {
        nav_activate(fm);
    }
}

static void on_scroll(NvWindow *win, NvScrollEvent *ev, void *userdata) {
    (void)win;
    FileManager *fm = (FileManager *)userdata;
    if (ev->x < SIDEBAR_W) return;
    dir_view_scroll(fm->view, ev->delta_y);
    nv_surface_invalidate(fm->surface);
}

static void on_key_down(NvWindow *win, NvKeyEvent *ev, void *userdata) {
    (void)win;
    FileManager *fm = (FileManager *)userdata;

    if (ev->mod & NV_MOD_CTRL) {
        switch (ev->key) {
            case 'c': clip_copy(fm);          return;
            case 'x': clip_cut(fm);           return;
            case 'v': clip_paste(fm);         return;
            case 'r': dir_view_refresh(fm->view);
                      update_status_for_dir(fm);
                      nv_surface_invalidate(fm->surface);
                      return;
            case 'h': fm->show_hidden = !fm->show_hidden;
                      dir_view_toggle_hidden(fm->view);
                      update_status_for_dir(fm);
                      nv_surface_invalidate(fm->surface);
                      return;
            case 'n': action_new_folder(fm);  return;
            default:  break;
        }
    }

    switch (ev->key) {
        case NV_KEY_F2:      action_rename(fm);    return;
        case NV_KEY_DELETE:  action_delete(fm);    return;
        case NV_KEY_F5:      dir_view_refresh(fm->view);
                             update_status_for_dir(fm);
                             nv_surface_invalidate(fm->surface);
                             return;
        case NV_KEY_ALT_LEFT:  nav_back(fm);       return;
        case NV_KEY_ALT_RIGHT: nav_forward(fm);    return;
        case NV_KEY_F10:     action_properties(fm);return;
        default: break;
    }

    int action = dir_view_key_down(fm->view, ev->key);
    switch (action) {
        case DV_ACTION_SELECT:
            update_status_for_dir(fm);
            nv_surface_invalidate(fm->surface);
            break;
        case DV_ACTION_ACTIVATE:
            nav_activate(fm);
            break;
        case DV_ACTION_GO_UP:
            nav_up(fm);
            break;
        case DV_ACTION_REFRESH:
            update_status_for_dir(fm);
            nv_surface_invalidate(fm->surface);
            break;
        default:
            break;
    }
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(int argc, char **argv) {
    memset(&g_fm, 0, sizeof(FileManager));

    const char *start_path = argc > 1 ? argv[1] : "/home";

    g_fm.view = dir_view_new(CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H);
    if (!g_fm.view) return 1;

    bookmarks_init(&g_fm);

    NvWindowConfig cfg;
    cfg.title    = "File Manager";
    cfg.width    = WINDOW_W;
    cfg.height   = WINDOW_H;
    cfg.resizable = 0;

    g_fm.window = nv_window_create(&cfg);
    if (!g_fm.window) return 1;

    nv_window_on_paint(g_fm.window,     on_paint,     &g_fm);
    nv_window_on_mouse_down(g_fm.window, on_mouse_down, &g_fm);
    nv_window_on_scroll(g_fm.window,    on_scroll,    &g_fm);
    nv_window_on_key_down(g_fm.window,  on_key_down,  &g_fm);

    nav_go(&g_fm, start_path);
    set_status(&g_fm, "Ready");

    return nv_app_run();
}