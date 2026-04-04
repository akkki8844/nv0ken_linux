#ifndef DIR_VIEW_H
#define DIR_VIEW_H

#include <stddef.h>
#include <sys/stat.h>
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../libnv0/include/nv0/window.h"
#include "../../../libnv0/include/nv0/input.h"

typedef enum {
    FTYPE_DIR,
    FTYPE_FILE,
    FTYPE_EXEC,
    FTYPE_LINK,
    FTYPE_IMAGE,
    FTYPE_TEXT,
} FileType;

typedef enum {
    SORT_NAME,
    SORT_SIZE,
    SORT_DATE,
    SORT_TYPE,
} DirViewSortMode;

typedef enum {
    DV_ACTION_NONE,
    DV_ACTION_SELECT,
    DV_ACTION_ACTIVATE,
    DV_ACTION_GO_UP,
    DV_ACTION_REFRESH,
} DirViewAction;

typedef struct {
    char      *name;
    char      *path;
    char      *ext;
    char      *size_str;
    char      *date_str;
    FileType   type;
    long long  size_bytes;
    long       mtime;
    int        mode;
} DirEntry;

typedef struct {
    int              x, y, w, h;

    DirEntry        *entries;
    int              count;
    int              cap;

    char            *path;
    int              selected;
    int              scroll_y;

    DirViewSortMode  sort_mode;
    int              sort_asc;
    int              show_hidden;
} DirView;

DirView    *dir_view_new(int x, int y, int w, int h);
void        dir_view_free(DirView *dv);

int         dir_view_load(DirView *dv, const char *path);
void        dir_view_refresh(DirView *dv);
void        dir_view_set_sort(DirView *dv, DirViewSortMode mode, int ascending);
void        dir_view_toggle_hidden(DirView *dv);

void        dir_view_draw(DirView *dv, NvSurface *surface);

int         dir_view_mouse_down(DirView *dv, int x, int y);
int         dir_view_scroll(DirView *dv, int delta_y);
int         dir_view_key_down(DirView *dv, int key);

DirEntry   *dir_view_selected_entry(DirView *dv);
const char *dir_view_current_path(DirView *dv);
int         dir_view_entry_count(DirView *dv);
void        dir_view_select_by_name(DirView *dv, const char *name);

#endif