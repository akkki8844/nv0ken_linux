#include "tcp.h"

#include "../lib/string.h"

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t data_offset;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header_t;

void tcp_init_socket(tcp_socket_t *socket, uint16_t local_port)
{
    if (!socket) {
        return;
    }

    memset(socket, 0, sizeof(*socket));
    socket->state = TCP_CLOSED;
    socket->local_port = local_port;
}

void tcp_receive(netdev_t *dev, const ipv4_header_t *ip, const void *payload, size_t length)
{
    (void)dev;
    (void)ip;
    if (!payload || length < sizeof(tcp_header_t)) {
        return;
    }
}
