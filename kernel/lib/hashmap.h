#ifndef NV0KEN_LIB_HASHMAP_H
#define NV0KEN_LIB_HASHMAP_H

#include <stddef.h>
#include <stdint.h>

typedef struct hashmap_entry {
    const char *key;
    void *value;
    uint64_t hash;
    struct hashmap_entry *next;
} hashmap_entry_t;

typedef struct {
    hashmap_entry_t **buckets;
    size_t bucket_count;
    hashmap_entry_t *entries;
    size_t entry_capacity;
    size_t entry_count;
} hashmap_t;

uint64_t hashmap_hash_string(const char *key);
void hashmap_init(hashmap_t *map,
                  hashmap_entry_t **buckets, size_t bucket_count,
                  hashmap_entry_t *entries, size_t entry_capacity);
void hashmap_clear(hashmap_t *map);
int  hashmap_put(hashmap_t *map, const char *key, void *value);
void *hashmap_get(const hashmap_t *map, const char *key);
int  hashmap_remove(hashmap_t *map, const char *key);
size_t hashmap_size(const hashmap_t *map);

#endif
