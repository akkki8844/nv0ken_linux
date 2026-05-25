#ifndef NV0KEN_MM_HEAP_H
#define NV0KEN_MM_HEAP_H

#include <stddef.h>

void heap_init(void *base, size_t size);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);
size_t heap_bytes_used(void);
size_t heap_bytes_free(void);

#endif
