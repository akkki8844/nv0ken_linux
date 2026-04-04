#include "http_client.h"
#include "html_parser.h"
#include "renderer.h"
#include "../../../libnv0/include/nv0/app.h"
#include "../../../libnv0/include/nv0/window.h"
#include "../../../libnv0/include/nv0/input.h"
#include "../../../libnv0/include/nv0/draw.h"
#include "../../../libnv0/include/nv0/dialog.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WINDOW_W        1024
#define WINDOW_H        768
#define TOOLBAR_H       40
#define STATUSBAR_H     24
#define URL_BAR_X       80
#define URL_BAR_W       (WINDOW_W - 180)
#define MAX_URL_LEN     2048
#define MAX_HISTORY     64

typedef enum {
    PAGE_IDLE,
    PAGE_LOADING,
    PAGE_DONE,
    PAGE_ERROR,
} PageState;

typedef struct {
    char *url;
    char *title;
} HistoryEntry;

typedef struct {
    NvWindow    *window;
    NvSurface   *surface;

    char         url_bar[MAX_URL_LEN];
    int          url_bar_focused;
    int          url_bar_cursor;

    PageState    state;
    char        *status_text;

    HtmlDocument *doc;
    RenderOutput *render;
    int           scroll_y;

    HistoryEntry  history[MAX_HISTORY];
    int           history_count;
    int           history_pos;
} Browser;

static Browser g_browser;

static void browser_set_status(Browser *b, const char *text) {
    free(b->status_text);
    b->status_text = text ? strdup(text) : NULL;
}

static void browser_push_history(Browser *b, const char *url) {
    while (b->history_count > b->history_pos + 1) {
        b->history_count--;
        free(b->history[b->history_count].url);
        free(b->history[b->history_count].title);
        b->history[b->history_count].url   = NULL;
        b->history[b->history_count].title = NULL;
    }

    if (b->history_count >= MAX_HISTORY) {
        free(b->history[0].url);
        free(b->history[0].title);
        memmove(&b->history[0], &b->history[1],
                sizeof(HistoryEntry) * (MAX_HISTORY - 1));
        b->history_count--;
    }

    b->history[b->history_count].url   = strdup(url);
    b->history[b->history_count].title = NULL;
    b->history_pos   = b->history_count;
    b->history_count++;
}

static void browser_navigate(Browser *b, const char *url) {
    if (!url || !url[0]) return;

    strncpy(b->url_bar, url, MAX_URL_LEN - 1);
    b->url_bar[MAX_URL_LEN - 1] = '\0';
    b->url_bar_focused = 0;
    b->url_bar_cursor  = strlen(b->url_bar);
    b->scroll_y        = 0;
    b->state           = PAGE_LOADING;

    browser_set_status(b, "Loading...");
    nv_surface_invalidate(b->surface);

    if (b->doc) {
        html_document_free(b->doc);
        b->doc = NULL;
    }
    if (b->render) {
        renderer_output_free(b->render);
        b->render = NULL;
    }

    HttpResponse *res = http_get(url);

    if (!res || res->error != HTTP_OK) {
        b->state = PAGE_ERROR;
        browser_set_status(b, res ? http_error_str(res->error) : "request failed");
        if (res) http_response_free(res);
        nv_surface_invalidate(b->surface);
        return;
    }

    if (res->status_code >= 400) {
        char msg[64];
        snprintf(msg, sizeof(msg), "HTTP %d %s",
                 res->status_code,
                 res->status_text ? res->status_text : "");
        b->state = PAGE_ERROR;
        browser_set_status(b, msg);
        http_response_free(res);
        nv_surface_invalidate(b->surface);
        return;
    }

    if (res->final_url && strcmp(res->final_url, url) != 0) {
        strncpy(b->url_bar, res->final_url, MAX_URL_LEN - 1);
        b->url_bar[MAX_URL_LEN - 1] = '\0';
    }

    browser_push_history(b, b->url_bar);

    if (res->body && res->body_len > 0) {
        b->doc = html_parse(res->body, res->body_len);
    }

    http_response_free(res);

    if (b->doc) {
        RenderConfig cfg;
        cfg.viewport_w  = WINDOW_W;
        cfg.viewport_h  = WINDOW_H - TOOLBAR_H - STATUSBAR_H;
        cfg.base_font_size = 14;
        b->render = renderer_render(b->doc, &cfg);

        HtmlNode *title_node = html_query_selector(b->doc->root, "title");
        if (title_node) {
            char *title_text = html_text_content(title_node);
            if (title_text) {
                nv_window_set_title(b->window, title_text);
                if (b->history_pos >= 0 && b->history_pos < b->history_count) {
                    free(b->history[b->history_pos].title);
                    b->history[b->history_pos].title = strdup(title_text);
                }
                free(title_text);
            }
        }
    }

    b->state = PAGE_DONE;
    browser_set_status(b, "Done");
    nv_surface_invalidate(b->surface);
}

static void browser_go_back(Browser *b) {
    if (b->history_pos <= 0) return;
    b->history_pos--;
    const char *url = b->history[b->history_pos].url;
    if (url) {
        strncpy(b->url_bar, url, MAX_URL_LEN - 1);
        browser_navigate(b, url);
    }
}

static void browser_go_forward(Browser *b) {
    if (b->history_pos >= b->history_count - 1) return;
    b->history_pos++;
    const char *url = b->history[b->history_pos].url;
    if (url) {
        strncpy(b->url_bar, url, MAX_URL_LEN - 1);
        browser_navigate(b, url);
    }
}

static void browser_reload(Browser *b) {
    if (b->url_bar[0]) browser_navigate(b, b->url_bar);
}

/* -----------------------------------------------------------------------
 * Drawing
 * --------------------------------------------------------------------- */

static void draw_toolbar(Browser *b) {
    NvRect bar = {0, 0, WINDOW_W, TOOLBAR_H};
    nv_draw_fill_rect(b->surface, &bar, NV_COLOR(0xF0, 0xF0, 0xF0, 0xFF));

    NvRect border = {0, TOOLBAR_H - 1, WINDOW_W, 1};
    nv_draw_fill_rect(b->surface, &border, NV_COLOR(0xCC, 0xCC, 0xCC, 0xFF));

    NvRect back_btn = {4, 6, 28, 28};
    int can_back    = b->history_pos > 0;
    nv_draw_fill_rect(b->surface, &back_btn,
                      can_back ? NV_COLOR(0xDD,0xDD,0xDD,0xFF)
                               : NV_COLOR(0xEE,0xEE,0xEE,0xFF));
    nv_draw_text(b->surface, 10, 10, "<",
                 can_back ? NV_COLOR(0x00,0x00,0x00,0xFF)
                          : NV_COLOR(0xAA,0xAA,0xAA,0xFF));

    NvRect fwd_btn = {36, 6, 28, 28};
    int can_fwd    = b->history_pos < b->history_count - 1;
    nv_draw_fill_rect(b->surface, &fwd_btn,
                      can_fwd ? NV_COLOR(0xDD,0xDD,0xDD,0xFF)
                              : NV_COLOR(0xEE,0xEE,0xEE,0xFF));
    nv_draw_text(b->surface, 42, 10, ">",
                 can_fwd ? NV_COLOR(0x00,0x00,0x00,0xFF)
                         : NV_COLOR(0xAA,0xAA,0xAA,0xFF));

    NvRect url_bg = {URL_BAR_X, 6, URL_BAR_W, 28};
    nv_draw_fill_rect(b->surface, &url_bg, NV_COLOR(0xFF,0xFF,0xFF,0xFF));
    nv_draw_rect(b->surface, &url_bg,
                 b->url_bar_focused ? NV_COLOR(0x00,0x78,0xD7,0xFF)
                                    : NV_COLOR(0xAA,0xAA,0xAA,0xFF));
    nv_draw_text_clip(b->surface, URL_BAR_X + 6, 12,
                      b->url_bar, NV_COLOR(0x00,0x00,0x00,0xFF),
                      URL_BAR_W - 12);

    NvRect go_btn = {URL_BAR_X + URL_BAR_W + 4, 6, 56, 28};
    nv_draw_fill_rect(b->surface, &go_btn, NV_COLOR(0x00,0x78,0xD7,0xFF));
    nv_draw_text(b->surface, URL_BAR_X + URL_BAR_W + 16, 12, "Go",
                 NV_COLOR(0xFF,0xFF,0xFF,0xFF));
}

static void draw_statusbar(Browser *b) {
    int y = WINDOW_H - STATUSBAR_H;
    NvRect bar = {0, y, WINDOW_W, STATUSBAR_H};
    nv_draw_fill_rect(b->surface, &bar, NV_COLOR(0xF0,0xF0,0xF0,0xFF));
    NvRect border = {0, y, WINDOW_W, 1};
    nv_draw_fill_rect(b->surface, &border, NV_COLOR(0xCC,0xCC,0xCC,0xFF));
    if (b->status_text)
        nv_draw_text(b->surface, 8, y + 5, b->status_text,
                     NV_COLOR(0x44,0x44,0x44,0xFF));
}

static void draw_page(Browser *b) {
    int content_y = TOOLBAR_H;
    int content_h = WINDOW_H - TOOLBAR_H - STATUSBAR_H;

    NvRect content = {0, content_y, WINDOW_W, content_h};
    nv_draw_fill_rect(b->surface, &content, NV_COLOR(0xFF,0xFF,0xFF,0xFF));

    if (b->state == PAGE_LOADING) {
        nv_draw_text(b->surface, WINDOW_W / 2 - 30, content_y + content_h / 2,
                     "Loading...", NV_COLOR(0x44,0x44,0x44,0xFF));
        return;
    }

    if (b->state == PAGE_ERROR) {
        nv_draw_text(b->surface, 20, content_y + 20,
                     b->status_text ? b->status_text : "Error",
                     NV_COLOR(0xCC,0x00,0x00,0xFF));
        return;
    }

    if (b->render) {
        NvRect clip = {0, content_y, WINDOW_W, content_h};
        nv_surface_push_clip(b->surface, &clip);
        renderer_draw(b->surface, b->render, 0, content_y - b->scroll_y);
        nv_surface_pop_clip(b->surface);
    }
}

static void on_paint(NvWindow *win, NvSurface *surface, void *userdata) {
    (void)win;
    Browser *b = (Browser *)userdata;
    b->surface = surface;

    NvRect bg = {0, 0, WINDOW_W, WINDOW_H};
    nv_draw_fill_rect(surface, &bg, NV_COLOR(0xFF,0xFF,0xFF,0xFF));

    draw_toolbar(b);
    draw_page(b);
    draw_statusbar(b);
}

/* -----------------------------------------------------------------------
 * Input
 * --------------------------------------------------------------------- */

static int hit_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void on_mouse_down(NvWindow *win, NvMouseEvent *ev, void *userdata) {
    (void)win;
    Browser *b = (Browser *)userdata;

    if (hit_rect(ev->x, ev->y, 4, 6, 28, 28)) {
        browser_go_back(b);
        return;
    }
    if (hit_rect(ev->x, ev->y, 36, 6, 28, 28)) {
        browser_go_forward(b);
        return;
    }
    if (hit_rect(ev->x, ev->y, URL_BAR_X + URL_BAR_W + 4, 6, 56, 28)) {
        browser_navigate(b, b->url_bar);
        return;
    }
    if (hit_rect(ev->x, ev->y, URL_BAR_X, 6, URL_BAR_W, 28)) {
        b->url_bar_focused = 1;
        b->url_bar_cursor  = strlen(b->url_bar);
        nv_surface_invalidate(b->surface);
        return;
    }

    b->url_bar_focused = 0;
    nv_surface_invalidate(b->surface);

    if (b->render && ev->y >= TOOLBAR_H && ev->y < WINDOW_H - STATUSBAR_H) {
        int page_y = ev->y - TOOLBAR_H + b->scroll_y;
        const char *href = renderer_hit_test(b->render, ev->x, page_y);
        if (href) {
            char resolved[MAX_URL_LEN];
            if (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0) {
                strncpy(resolved, href, MAX_URL_LEN - 1);
            } else if (href[0] == '/') {
                HttpUrl *base = http_url_parse(b->url_bar);
                if (base) {
                    snprintf(resolved, MAX_URL_LEN, "%s://%s%s",
                             base->scheme, base->host, href);
                    http_url_free(base);
                } else {
                    strncpy(resolved, href, MAX_URL_LEN - 1);
                }
            } else {
                char *last_slash = strrchr(b->url_bar, '/');
                if (last_slash) {
                    size_t base_len = last_slash - b->url_bar + 1;
                    snprintf(resolved, MAX_URL_LEN, "%.*s%s",
                             (int)base_len, b->url_bar, href);
                } else {
                    snprintf(resolved, MAX_URL_LEN, "%s/%s", b->url_bar, href);
                }
            }
            resolved[MAX_URL_LEN - 1] = '\0';
            browser_navigate(b, resolved);
        }
    }
}

static void on_scroll(NvWindow *win, NvScrollEvent *ev, void *userdata) {
    (void)win;
    Browser *b = (Browser *)userdata;
    b->scroll_y += ev->delta_y * 40;
    if (b->scroll_y < 0) b->scroll_y = 0;
    if (b->render) {
        int max_scroll = renderer_page_height(b->render) - (WINDOW_H - TOOLBAR_H - STATUSBAR_H);
        if (max_scroll < 0) max_scroll = 0;
        if (b->scroll_y > max_scroll) b->scroll_y = max_scroll;
    }
    nv_surface_invalidate(b->surface);
}

static void on_key_down(NvWindow *win, NvKeyEvent *ev, void *userdata) {
    (void)win;
    Browser *b = (Browser *)userdata;

    if (!b->url_bar_focused) {
        if (ev->key == NV_KEY_F5)       browser_reload(b);
        if (ev->key == NV_KEY_ALT_LEFT) browser_go_back(b);
        if (ev->key == NV_KEY_ALT_RIGHT)browser_go_forward(b);
        return;
    }

    int len = strlen(b->url_bar);

    switch (ev->key) {
        case NV_KEY_RETURN:
            browser_navigate(b, b->url_bar);
            break;
        case NV_KEY_ESCAPE:
            b->url_bar_focused = 0;
            nv_surface_invalidate(b->surface);
            break;
        case NV_KEY_BACKSPACE:
            if (b->url_bar_cursor > 0) {
                memmove(b->url_bar + b->url_bar_cursor - 1,
                        b->url_bar + b->url_bar_cursor,
                        len - b->url_bar_cursor + 1);
                b->url_bar_cursor--;
                nv_surface_invalidate(b->surface);
            }
            break;
        case NV_KEY_DELETE:
            if (b->url_bar_cursor < len) {
                memmove(b->url_bar + b->url_bar_cursor,
                        b->url_bar + b->url_bar_cursor + 1,
                        len - b->url_bar_cursor);
                nv_surface_invalidate(b->surface);
            }
            break;
        case NV_KEY_LEFT:
            if (b->url_bar_cursor > 0) { b->url_bar_cursor--; nv_surface_invalidate(b->surface); }
            break;
        case NV_KEY_RIGHT:
            if (b->url_bar_cursor < len) { b->url_bar_cursor++; nv_surface_invalidate(b->surface); }
            break;
        case NV_KEY_HOME:
            b->url_bar_cursor = 0;
            nv_surface_invalidate(b->surface);
            break;
        case NV_KEY_END:
            b->url_bar_cursor = len;
            nv_surface_invalidate(b->surface);
            break;
        default:
            if (ev->codepoint >= 32 && ev->codepoint < 127 && len < MAX_URL_LEN - 1) {
                memmove(b->url_bar + b->url_bar_cursor + 1,
                        b->url_bar + b->url_bar_cursor,
                        len - b->url_bar_cursor + 1);
                b->url_bar[b->url_bar_cursor] = (char)ev->codepoint;
                b->url_bar_cursor++;
                nv_surface_invalidate(b->surface);
            }
            break;
    }
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int main(int argc, char **argv) {
    memset(&g_browser, 0, sizeof(Browser));
    browser_set_status(&g_browser, "Ready");

    NvWindowConfig cfg;
    cfg.title   = "nv0ken browser";
    cfg.width   = WINDOW_W;
    cfg.height  = WINDOW_H;
    cfg.resizable = 0;

    g_browser.window = nv_window_create(&cfg);
    if (!g_browser.window) return 1;

    nv_window_on_paint(g_browser.window,    on_paint,      &g_browser);
    nv_window_on_mouse_down(g_browser.window, on_mouse_down, &g_browser);
    nv_window_on_scroll(g_browser.window,   on_scroll,     &g_browser);
    nv_window_on_key_down(g_browser.window, on_key_down,   &g_browser);

    const char *start_url = argc > 1 ? argv[1] : "http://example.com";
    strncpy(g_browser.url_bar, start_url, MAX_URL_LEN - 1);
    browser_navigate(&g_browser, start_url);

    return nv_app_run();
}