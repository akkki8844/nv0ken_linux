#ifndef NV0KEN_NET_SOCKET_H
#define NV0KEN_NET_SOCKET_H

#include <stdint.h>

typedef enum {
    SOCKET_UNUSED,
    SOCKET_UDP,
    SOCKET_TCP
} socket_type_t;

typedef struct {
    socket_type_t type;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
} socket_t;

void socket_init(void);
int socket_create(socket_type_t type);
socket_t *socket_get(int fd);
int socket_close(int fd);
int socket_connect(int fd, uint32_t remote_ip, uint16_t remote_port);

#endif
