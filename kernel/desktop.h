#ifndef NV0KEN_DESKTOP_H
#define NV0KEN_DESKTOP_H

#include <stdint.h>

/* Draw the kernel-owned recovery desktop. It remains usable even when the
 * userspace compositor and process launcher are unavailable. */
void desktop_render(uint32_t pci_devices);

#endif
