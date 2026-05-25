#include "socket.h"

#include "../lib/string.h"

#define SOCKET_MAX 64

static socket_t sockets[SOCKET_MAX];

void socket_init(void)
{
    memset(sockets, 0, sizeof(sockets));
}

int socket_create(socket_type_t type)
{
    if (type == SOCKET_UNUSED) {
        return -1;
    }

    for (int index = 0; index < SOCKET_MAX; ++index) {
        if (sockets[index].type == SOCKET_UNUSED) {
            sockets[index].type = type;
            return index;
        }
    }
    return -1;
}

socket_t *socket_get(int fd)
{
    if (fd < 0 || fd >= SOCKET_MAX || sockets[fd].type == SOCKET_UNUSED) {
        return 0;
    }
    return &sockets[fd];
}

int socket_close(int fd)
{
    socket_t *socket = socket_get(fd);
    if (!socket) {
        return -1;
    }
    memset(socket, 0, sizeof(*socket));
    return 0;
}

int socket_connect(int fd, uint32_t remote_ip, uint16_t remote_port)
{
    socket_t *socket = socket_get(fd);
    if (!socket) {
        return -1;
    }
    socket->remote_ip = remote_ip;
    socket->remote_port = remote_port;
    return 0;
}
