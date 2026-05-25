#include "dhcp.h"

#include "../lib/string.h"
#include "udp.h"

int dhcp_discover(netdev_t *dev)
{
    if (!dev) {
        return -1;
    }

    unsigned char packet[244];
    memset(packet, 0, sizeof(packet));
    packet[0] = 1;
    packet[1] = 1;
    packet[2] = 6;
    packet[236] = 99;
    packet[237] = 130;
    packet[238] = 83;
    packet[239] = 99;
    packet[240] = 53;
    packet[241] = 1;
    packet[242] = 1;
    packet[243] = 255;
    return udp_send(dev, 0xFFFFFFFFu, 68, 67, packet, sizeof(packet));
}
