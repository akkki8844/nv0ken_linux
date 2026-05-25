#include "netdev.h"

#include "../lib/string.h"

static netdev_t *devices[NETDEV_MAX];

void netdev_init(void)
{
    memset(devices, 0, sizeof(devices));
}

int netdev_register(netdev_t *dev)
{
    if (!dev || !dev->send) {
        return -1;
    }

    for (size_t index = 0; index < NETDEV_MAX; ++index) {
        if (!devices[index]) {
            devices[index] = dev;
            return 0;
        }
    }
    return -1;
}

netdev_t *netdev_default(void)
{
    return devices[0];
}

netdev_t *netdev_find(const char *name)
{
    if (!name) {
        return 0;
    }

    for (size_t index = 0; index < NETDEV_MAX; ++index) {
        if (devices[index] && strcmp(devices[index]->name, name) == 0) {
            return devices[index];
        }
    }
    return 0;
}

int netdev_send(netdev_t *dev, const void *data, size_t length)
{
    if (!dev) {
        dev = netdev_default();
    }
    if (!dev || !dev->send || (!data && length)) {
        return -1;
    }
    return dev->send(dev, data, length);
}

void netdev_poll_all(void)
{
    for (size_t index = 0; index < NETDEV_MAX; ++index) {
        if (devices[index] && devices[index]->poll) {
            devices[index]->poll(devices[index]);
        }
    }
}
