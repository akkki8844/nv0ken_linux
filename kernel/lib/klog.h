#ifndef NV0KEN_LIB_KLOG_H
#define NV0KEN_LIB_KLOG_H

#include <stddef.h>

void klog_clear(void);
void klog_putc(char ch);
size_t klog_read(size_t offset, void *buffer, size_t count);
size_t klog_size(void);

#endif
