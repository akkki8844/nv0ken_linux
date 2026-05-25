#ifndef NV0KEN_MM_SLAB_H
#define NV0KEN_MM_SLAB_H

#include <stddef.h>

typedef struct slab_cache {
    const char *name;
    size_t object_size;
    void *free_list;
    size_t allocated;
    size_t freed;
} slab_cache_t;

void slab_cache_init(slab_cache_t *cache, const char *name, size_t object_size);
void *slab_alloc(slab_cache_t *cache);
void slab_free(slab_cache_t *cache, void *object);
size_t slab_live_count(const slab_cache_t *cache);

#endif
