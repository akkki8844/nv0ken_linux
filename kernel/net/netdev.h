#ifndef NV0KEN_NETDEV_H
#define NV0KEN_NETDEV_H

#include <stddef.h>
#include <stdint.h>

#define NETDEV_NAME_MAX 16
#define NETDEV_MAX 8

typedef struct netdev {
    char name[NETDEV_NAME_MAX];
    uint8_t mac[6];
    uint32_t ipv4_addr;
    uint32_t ipv4_gateway;
    uint32_t ipv4_netmask;
    int (*send)(struct netdev *dev, const void *data, size_t length);
    void (*poll)(struct netdev *dev);
    void *driver_data;
} netdev_t;

void netdev_init(void);
int netdev_register(netdev_t *dev);
netdev_t *netdev_default(void);
netdev_t *netdev_find(const char *name);
int netdev_send(netdev_t *dev, const void *data, size_t length);
void netdev_poll_all(void);

#endif
