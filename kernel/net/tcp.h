#ifndef NV0KEN_NET_TCP_H
#define NV0KEN_NET_TCP_H

#include <stddef.h>
#include <stdint.h>

#include "ipv4.h"
#include "tcp_states.h"

typedef struct {
    tcp_state_t state;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t send_next;
    uint32_t recv_next;
} tcp_socket_t;

void tcp_init_socket(tcp_socket_t *socket, uint16_t local_port);
void tcp_receive(netdev_t *dev, const ipv4_header_t *ip, const void *payload, size_t length);

#endif
