#ifndef MENU_H
#define MENU_H

#include "widget.h"

#define MENU_MAX_ITEMS   32
#define MENU_ITEM_H      26
#define MENU_MIN_W       160
#define MENU_PAD_X       16
#define MENU_SHORTCUT_W  60

typedef enum {
    MENU_ITEM_NORMAL,
    MENU_ITEM_SEPARATOR,
    MENU_ITEM_SUBMENU,
    MENU_ITEM_CHECK,
} MenuItemType;

typedef struct MenuItem {
    MenuItemType  type;
    char          label[64];
    char          shortcut[16];
    int           enabled;
    int           checked;
    int           hovered;
    void (*action)(void *ud);
    void         *action_ud;
    struct Menu  *submenu;
} MenuItem;

typedef struct Menu {
    Widget    base;
    MenuItem  items[MENU_MAX_ITEMS];
    int       item_count;
    int       hovered;
    int       visible;
    int       shadow;
    struct Menu *parent_menu;
    int        open_submenu_idx;
} Menu;

Menu     *menu_new(void);
void      menu_free(Menu *m);
void      menu_add_item(Menu *m, const char *label, const char *shortcut,
                         void (*action)(void *), void *ud);
void      menu_add_check(Menu *m, const char *label, int *checked_ptr);
void      menu_add_separator(Menu *m);
MenuItem *menu_add_submenu(Menu *m, const char *label, Menu *submenu);
void      menu_show(Menu *m, int x, int y, NvSurface *surface);
void      menu_hide(Menu *m);
int       menu_visible(const Menu *m);
void      menu_draw(Widget *w, NvSurface *dst);
void      menu_on_event(Widget *w, const WidgetEvent *ev, void *ud);
int       menu_item_count(const Menu *m);

#endif