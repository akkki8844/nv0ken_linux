#include "../include/nv0/ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../../graphics/server/ipc_server.h"
#include "internal.h"

static int ipc_in_fd = -1;
static int ipc_out_fd = -1;
static char clipboard_cache[4096];

static int write_all(int fd, const void *buffer, size_t length)
{
    const unsigned char *bytes = buffer;
    size_t done = 0;
    while (done < length) {
        ssize_t n = write(fd, bytes + done, length - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

static int create_endpoint(const char *path, mode_t mode)
{
    int fd = open(path, O_CREAT | O_RDWR, mode);
    if (fd < 0) {
        return -1;
    }
    close(fd);
    return 0;
}

int nv_ipc_connect(void)
{
    if (ipc_in_fd >= 0) {
        return 0;
    }

    int pid = (int)getpid();
    char in_path[128];
    char out_path[128];
    snprintf(in_path, sizeof(in_path), "/tmp/nv0_client_%d_in", pid);
    snprintf(out_path, sizeof(out_path), "/tmp/nv0_client_%d_out", pid);

    unlink(in_path);
    unlink(out_path);
    if (create_endpoint(in_path, 0600) < 0 && errno != EEXIST) {
        return -1;
    }
    if (create_endpoint(out_path, 0600) < 0 && errno != EEXIST) {
        unlink(in_path);
        return -1;
    }

    int rendezvous = open(IPC_SOCKET_PATH, O_WRONLY | O_NONBLOCK);
    if (rendezvous < 0) {
        unlink(in_path);
        unlink(out_path);
        return -1;
    }
    if (write_all(rendezvous, &pid, sizeof(pid)) < 0) {
        close(rendezvous);
        unlink(in_path);
        unlink(out_path);
        return -1;
    }
    close(rendezvous);

    ipc_in_fd = open(in_path, O_WRONLY | O_NONBLOCK);
    ipc_out_fd = open(out_path, O_RDONLY | O_NONBLOCK);
    unlink(in_path);
    unlink(out_path);

    if (ipc_in_fd < 0) {
        if (ipc_out_fd >= 0) {
            close(ipc_out_fd);
            ipc_out_fd = -1;
        }
        return -1;
    }

    return 0;
}

int nv_ipc_fd(void)
{
    return ipc_out_fd;
}

int nv_ipc_send(uint32_t type, const void *payload, size_t payload_len)
{
    if (ipc_in_fd < 0 && nv_ipc_connect() < 0) {
        return -1;
    }

    IpcHeader header = {type, (uint32_t)payload_len};
    if (write_all(ipc_in_fd, &header, sizeof(header)) < 0) {
        return -1;
    }
    if (payload_len > 0 && payload) {
        return write_all(ipc_in_fd, payload, payload_len);
    }
    return 0;
}

static int dispatch_message(uint32_t type, const void *payload, uint32_t length)
{
    if (type == NV_MSG_CLIPBOARD_DATA && length >= sizeof(uint32_t)) {
        uint32_t text_len = 0;
        memcpy(&text_len, payload, sizeof(text_len));
        if (text_len >= sizeof(clipboard_cache)) {
            text_len = sizeof(clipboard_cache) - 1;
        }
        if (length >= sizeof(uint32_t) + text_len) {
            memcpy(clipboard_cache, (const char *)payload + sizeof(uint32_t), text_len);
            clipboard_cache[text_len] = '\0';
        }
        return 0;
    }

    return nv_window_dispatch(type, payload, length);
}

int nv_ipc_poll(int timeout_ms)
{
    (void)timeout_ms;
    if (ipc_out_fd < 0) {
        return 0;
    }

    for (;;) {
        IpcHeader header;
        ssize_t n = read(ipc_out_fd, &header, sizeof(header));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return 0;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        if (n != (ssize_t)sizeof(header) || header.length > IPC_RECV_BUF) {
            return -1;
        }

        unsigned char payload[IPC_RECV_BUF];
        size_t done = 0;
        while (done < header.length) {
            n = read(ipc_out_fd, payload + done, header.length - done);
            if (n <= 0) {
                return -1;
            }
            done += (size_t)n;
        }
        dispatch_message(header.type, payload, header.length);
    }
}

void nv_ipc_disconnect(void)
{
    if (ipc_in_fd >= 0) {
        close(ipc_in_fd);
        ipc_in_fd = -1;
    }
    if (ipc_out_fd >= 0) {
        close(ipc_out_fd);
        ipc_out_fd = -1;
    }
}

int nv_clipboard_set(const char *text)
{
    if (!text) {
        text = "";
    }
    size_t len = strlen(text);
    if (len >= sizeof(clipboard_cache)) {
        len = sizeof(clipboard_cache) - 1;
    }
    memcpy(clipboard_cache, text, len);
    clipboard_cache[len] = '\0';

    size_t total = sizeof(uint32_t) + len;
    unsigned char *payload = malloc(total);
    if (!payload) {
        return -1;
    }
    uint32_t ulen = (uint32_t)len;
    memcpy(payload, &ulen, sizeof(ulen));
    memcpy(payload + sizeof(ulen), text, len);
    int rc = nv_ipc_send(NV_MSG_CLIPBOARD_SET, payload, total);
    free(payload);
    return rc;
}

int nv_clipboard_get(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    nv_ipc_send(NV_MSG_CLIPBOARD_GET, 0, 0);
    nv_ipc_poll(0);
    size_t len = strlen(clipboard_cache);
    if (len >= buffer_size) {
        len = buffer_size - 1;
    }
    memcpy(buffer, clipboard_cache, len);
    buffer[len] = '\0';
    return (int)len;
}
