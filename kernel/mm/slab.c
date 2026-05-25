#include "slab.h"

#include "heap.h"
#include "lib/math.h"

typedef struct free_node {
    struct free_node *next;
} free_node_t;

void slab_cache_init(slab_cache_t *cache, const char *name, size_t object_size)
{
    if (!cache) {
        return;
    }
    cache->name = name;
    cache->object_size = MAX(object_size, sizeof(free_node_t));
    cache->free_list = 0;
    cache->allocated = 0;
    cache->freed = 0;
}

void *slab_alloc(slab_cache_t *cache)
{
    if (!cache) {
        return 0;
    }
    if (cache->free_list) {
        free_node_t *node = cache->free_list;
        cache->free_list = node->next;
        ++cache->allocated;
        return node;
    }
    void *object = kmalloc(cache->object_size);
    if (object) {
        ++cache->allocated;
    }
    return object;
}

void slab_free(slab_cache_t *cache, void *object)
{
    if (!cache || !object) {
        return;
    }
    free_node_t *node = object;
    node->next = cache->free_list;
    cache->free_list = node;
    ++cache->freed;
}

size_t slab_live_count(const slab_cache_t *cache)
{
    return cache ? cache->allocated - cache->freed : 0;
}
