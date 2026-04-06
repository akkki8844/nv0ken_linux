#include "ipc_server.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

/* nv0ken uses a minimal socket-like IPC built on kernel pipes/FIFOs
 * since BSD sockets are in the phase-6 network stack.
 * Each client connection is a pair of named FIFOs:
 *   /tmp/nv0_client_<pid>_in   (server reads, client writes)
 *   /tmp/nv0_client_<pid>_out  (server writes, client reads)
 * The rendezvous FIFO /tmp/nv0_compositor carries 4-byte client PIDs.
 */

#define RENDEZVOUS_FIFO IPC_SOCKET_PATH

struct IpcServer {
    int          rendezvous_fd;
    IpcClient    clients[IPC_MAX_CLIENTS];
    int          client_count;
    IpcCallbacks cbs;
    void        *userdata;
    int          running;
    char         clipboard[4096];
    int          clipboard_len;
};

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) return NULL;
    memset(p, 0, n);
    return p;
}

/* -----------------------------------------------------------------------
 * Low-level send helper
 * --------------------------------------------------------------------- */

static int ipc_write(int fd, const void *buf, size_t len) {
    const uint8_t *p = buf;
    size_t written   = 0;
    while (written < len) {
        ssize_t n = write(fd, p + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += n;
    }
    return 0;
}

static int send_message(IpcClient *client, uint32_t type,
                         const void *payload, uint32_t payload_len) {
    if (!client || client->fd < 0) return -1;
    IpcHeader hdr = { type, payload_len };
    if (ipc_write(client->fd, &hdr, sizeof(hdr)) < 0) return -1;
    if (payload_len > 0 && payload)
        if (ipc_write(client->fd, payload, payload_len) < 0) return -1;
    return 0;
}

/* -----------------------------------------------------------------------
 * Server lifecycle
 * --------------------------------------------------------------------- */

IpcServer *ipc_server_new(const IpcCallbacks *callbacks, void *userdata) {
    IpcServer *srv = xmalloc(sizeof(IpcServer));
    if (!srv) return NULL;
    if (callbacks) srv->cbs = *callbacks;
    srv->userdata       = userdata;
    srv->rendezvous_fd  = -1;
    return srv;
}

void ipc_server_free(IpcServer *srv) {
    if (!srv) return;
    ipc_server_stop(srv);
    free(srv);
}

int ipc_server_start(IpcServer *srv) {
    if (!srv) return -1;

    unlink(RENDEZVOUS_FIFO);
    if (mkfifo(RENDEZVOUS_FIFO, 0666) < 0 && errno != EEXIST) return -1;

    srv->rendezvous_fd = open(RENDEZVOUS_FIFO, O_RDONLY | O_NONBLOCK);
    if (srv->rendezvous_fd < 0) return -1;

    srv->running = 1;
    return 0;
}

void ipc_server_stop(IpcServer *srv) {
    if (!srv) return;
    srv->running = 0;

    for (int i = 0; i < srv->client_count; i++) {
        if (srv->clients[i].active && srv->clients[i].fd >= 0)
            close(srv->clients[i].fd);
    }
    srv->client_count = 0;

    if (srv->rendezvous_fd >= 0) {
        close(srv->rendezvous_fd);
        srv->rendezvous_fd = -1;
    }
    unlink(RENDEZVOUS_FIFO);
}

/* -----------------------------------------------------------------------
 * Accept new client
 * --------------------------------------------------------------------- */

static void accept_client(IpcServer *srv) {
    if (srv->client_count >= IPC_MAX_CLIENTS) return;

    int pid_raw = 0;
    ssize_t n = read(srv->rendezvous_fd, &pid_raw, 4);
    if (n != 4) return;

    char in_path[256], out_path[256];
    snprintf(in_path,  sizeof(in_path),  "/tmp/nv0_client_%d_in",  pid_raw);
    snprintf(out_path, sizeof(out_path), "/tmp/nv0_client_%d_out", pid_raw);

    int in_fd = open(in_path, O_RDONLY | O_NONBLOCK);
    if (in_fd < 0) return;

    IpcClient *c = &srv->clients[srv->client_count++];
    memset(c, 0, sizeof(IpcClient));
    c->fd     = in_fd;
    c->active = 1;

    /* We write back to the client on a separate out FIFO.
     * For simplicity store the out fd as negative of in fd index.
     * A real implementation would use bidirectional pipes. */
    (void)out_path;
}

/* -----------------------------------------------------------------------
 * Dispatch a fully-received message
 * --------------------------------------------------------------------- */

static void dispatch(IpcServer *srv, IpcClient *client,
                      uint32_t type, const uint8_t *payload, uint32_t len) {
    switch (type) {
        case NV_MSG_CREATE_WINDOW:
            if (len >= sizeof(IpcCreateWindow) && srv->cbs.on_create_window)
                srv->cbs.on_create_window(srv, client,
                                           (const IpcCreateWindow *)payload);
            break;
        case NV_MSG_DESTROY_WINDOW:
            if (len >= 4 && srv->cbs.on_destroy_window) {
                uint32_t wid;
                memcpy(&wid, payload, 4);
                srv->cbs.on_destroy_window(srv, client, wid);
            }
            break;
        case NV_MSG_SET_TITLE:
            if (len >= sizeof(IpcSetTitle) && srv->cbs.on_set_title)
                srv->cbs.on_set_title(srv, client,
                                       (const IpcSetTitle *)payload);
            break;
        case NV_MSG_RESIZE:
            if (len >= sizeof(IpcResize) && srv->cbs.on_resize)
                srv->cbs.on_resize(srv, client, (const IpcResize *)payload);
            break;
        case NV_MSG_MOVE:
            if (len >= sizeof(IpcMove) && srv->cbs.on_move)
                srv->cbs.on_move(srv, client, (const IpcMove *)payload);
            break;
        case NV_MSG_SHOW:
        case NV_MSG_HIDE:
            if (len >= 4 && srv->cbs.on_show_hide) {
                uint32_t wid;
                memcpy(&wid, payload, 4);
                srv->cbs.on_show_hide(srv, client, wid, type == NV_MSG_SHOW);
            }
            break;
        case NV_MSG_INVALIDATE:
            if (len >= 4 && srv->cbs.on_invalidate) {
                uint32_t wid;
                memcpy(&wid, payload, 4);
                srv->cbs.on_invalidate(srv, client, wid);
            }
            break;
        case NV_MSG_SWAP_BUFFER:
            if (len >= sizeof(IpcSwapBuffer) && srv->cbs.on_swap_buffer)
                srv->cbs.on_swap_buffer(srv, client,
                                         (const IpcSwapBuffer *)payload);
            break;
        case NV_MSG_CLIPBOARD_SET:
            if (len >= 4 && srv->cbs.on_clipboard_set) {
                uint32_t text_len;
                memcpy(&text_len, payload, 4);
                if (text_len > 0 && len >= 4 + text_len) {
                    srv->cbs.on_clipboard_set(srv, client,
                                               (const char *)(payload + 4),
                                               (int)text_len);
                }
            }
            break;
        case NV_MSG_CLIPBOARD_GET:
            ipc_send_clipboard_data(client,
                                     srv->clipboard, srv->clipboard_len);
            break;
        default:
            ipc_send_error(client, 0, EINVAL, "unknown message type");
            break;
    }
}

/* -----------------------------------------------------------------------
 * Poll loop — call once per frame
 * --------------------------------------------------------------------- */

int ipc_server_poll(IpcServer *srv, int timeout_ms) {
    if (!srv || !srv->running) return -1;
    (void)timeout_ms;

    accept_client(srv);

    for (int i = 0; i < srv->client_count; i++) {
        IpcClient *c = &srv->clients[i];
        if (!c->active || c->fd < 0) continue;

        uint8_t tmp[IPC_RECV_BUF];
        ssize_t n = read(c->fd, tmp,
                         IPC_RECV_BUF - c->recv_len);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            c->active = 0;
            close(c->fd);
            c->fd = -1;
            continue;
        }
        if (n == 0) {
            c->active = 0;
            close(c->fd);
            c->fd = -1;
            continue;
        }

        memcpy(c->recv_buf + c->recv_len, tmp, n);
        c->recv_len += n;

        while (c->recv_len >= (int)sizeof(IpcHeader)) {
            IpcHeader hdr;
            memcpy(&hdr, c->recv_buf, sizeof(IpcHeader));
            int total = (int)sizeof(IpcHeader) + (int)hdr.length;
            if (c->recv_len < total) break;

            dispatch(srv, c, hdr.type,
                     c->recv_buf + sizeof(IpcHeader), hdr.length);

            memmove(c->recv_buf, c->recv_buf + total,
                    c->recv_len - total);
            c->recv_len -= total;
        }
    }

    int alive = 0;
    for (int i = 0; i < srv->client_count; i++) {
        if (srv->clients[i].active)
            srv->clients[alive++] = srv->clients[i];
    }
    srv->client_count = alive;
    return 0;
}

void *ipc_server_userdata(IpcServer *srv) {
    return srv ? srv->userdata : NULL;
}

/* -----------------------------------------------------------------------
 * Send helpers
 * --------------------------------------------------------------------- */

int ipc_send_ack(IpcClient *c, uint32_t window_id, int ok) {
    IpcAck payload = { window_id, (uint32_t)(ok ? 1 : 0) };
    return send_message(c, NV_MSG_ACK, &payload, sizeof(payload));
}

int ipc_send_error(IpcClient *c, uint32_t window_id,
                   int errno_val, const char *msg) {
    IpcError payload;
    memset(&payload, 0, sizeof(payload));
    payload.window_id  = window_id;
    payload.errno_val  = errno_val;
    if (msg) strncpy(payload.message, msg, sizeof(payload.message) - 1);
    return send_message(c, NV_MSG_ERROR, &payload, sizeof(payload));
}

int ipc_send_paint(IpcClient *c, uint32_t window_id,
                   int x, int y, int w, int h) {
    IpcPaint payload = { window_id,
                          (uint32_t)x, (uint32_t)y,
                          (uint32_t)w, (uint32_t)h };
    return send_message(c, NV_MSG_PAINT, &payload, sizeof(payload));
}

int ipc_send_key(IpcClient *c, uint32_t window_id,
                 int type, uint32_t key, uint32_t cp, uint32_t mods) {
    IpcKeyEvent payload = { window_id, key, cp, mods };
    return send_message(c, (uint32_t)type, &payload, sizeof(payload));
}

int ipc_send_mouse(IpcClient *c, uint32_t window_id,
                   int type, int x, int y, uint32_t btn, uint32_t mods) {
    IpcMouseEvent payload = { window_id, x, y, btn, mods };
    return send_message(c, (uint32_t)type, &payload, sizeof(payload));
}

int ipc_send_scroll(IpcClient *c, uint32_t window_id, int dx, int dy) {
    IpcScrollEvent payload = { window_id, dx, dy };
    return send_message(c, NV_MSG_SCROLL, &payload, sizeof(payload));
}

int ipc_send_focus(IpcClient *c, uint32_t window_id, int gained) {
    IpcWindowId payload = { window_id };
    return send_message(c, gained ? NV_MSG_FOCUS_IN : NV_MSG_FOCUS_OUT,
                        &payload, sizeof(payload));
}

int ipc_send_close_request(IpcClient *c, uint32_t window_id) {
    IpcWindowId payload = { window_id };
    return send_message(c, NV_MSG_CLOSE_REQUEST, &payload, sizeof(payload));
}

int ipc_send_resize_notify(IpcClient *c, uint32_t window_id, int w, int h) {
    IpcResize payload = { window_id, (uint32_t)w, (uint32_t)h };
    return send_message(c, NV_MSG_RESIZE_NOTIFY, &payload, sizeof(payload));
}

int ipc_send_clipboard_data(IpcClient *c, const char *text, int len) {
    if (!c || len < 0) return -1;
    uint32_t ulen = (uint32_t)len;
    size_t total = sizeof(uint32_t) + (size_t)len;
    uint8_t *buf = malloc(total);
    if (!buf) return -1;
    memcpy(buf, &ulen, 4);
    if (len > 0 && text) memcpy(buf + 4, text, len);
    IpcHeader hdr = { NV_MSG_CLIPBOARD_DATA, (uint32_t)total };
    int r = ipc_write(c->fd, &hdr, sizeof(hdr));
    if (r == 0) r = ipc_write(c->fd, buf, total);
    free(buf);
    return r;
}