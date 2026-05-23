#include "hashmap.h"

#include "string.h"

uint64_t hashmap_hash_string(const char *key)
{
    uint64_t hash = 1469598103934665603ull;
    while (key && *key) {
        hash ^= (unsigned char)*key++;
        hash *= 1099511628211ull;
    }
    return hash;
}

void hashmap_init(hashmap_t *map,
                  hashmap_entry_t **buckets, size_t bucket_count,
                  hashmap_entry_t *entries, size_t entry_capacity)
{
    if (!map) {
        return;
    }
    map->buckets = buckets;
    map->bucket_count = bucket_count;
    map->entries = entries;
    map->entry_capacity = entry_capacity;
    map->entry_count = 0;
    hashmap_clear(map);
}

void hashmap_clear(hashmap_t *map)
{
    if (!map || !map->buckets) {
        return;
    }
    for (size_t i = 0; i < map->bucket_count; ++i) {
        map->buckets[i] = 0;
    }
    if (map->entries) {
        for (size_t i = 0; i < map->entry_capacity; ++i) {
            map->entries[i].key = 0;
            map->entries[i].value = 0;
            map->entries[i].hash = 0;
            map->entries[i].next = 0;
        }
    }
    map->entry_count = 0;
}

static int key_equals(const hashmap_entry_t *entry, const char *key, uint64_t hash)
{
    return entry->key && entry->hash == hash && strcmp(entry->key, key) == 0;
}

static hashmap_entry_t *alloc_entry(hashmap_t *map)
{
    if (!map || !map->entries || map->entry_count >= map->entry_capacity) {
        return 0;
    }

    for (size_t i = 0; i < map->entry_capacity; ++i) {
        if (!map->entries[i].key) {
            ++map->entry_count;
            return &map->entries[i];
        }
    }
    return 0;
}

int hashmap_put(hashmap_t *map, const char *key, void *value)
{
    if (!map || !map->buckets || map->bucket_count == 0 || !key) {
        return -1;
    }

    uint64_t hash = hashmap_hash_string(key);
    size_t bucket = hash % map->bucket_count;
    for (hashmap_entry_t *entry = map->buckets[bucket]; entry; entry = entry->next) {
        if (key_equals(entry, key, hash)) {
            entry->value = value;
            return 0;
        }
    }

    hashmap_entry_t *entry = alloc_entry(map);
    if (!entry) {
        return -1;
    }
    entry->key = key;
    entry->value = value;
    entry->hash = hash;
    entry->next = map->buckets[bucket];
    map->buckets[bucket] = entry;
    return 0;
}

void *hashmap_get(const hashmap_t *map, const char *key)
{
    if (!map || !map->buckets || map->bucket_count == 0 || !key) {
        return 0;
    }

    uint64_t hash = hashmap_hash_string(key);
    size_t bucket = hash % map->bucket_count;
    for (hashmap_entry_t *entry = map->buckets[bucket]; entry; entry = entry->next) {
        if (key_equals(entry, key, hash)) {
            return entry->value;
        }
    }
    return 0;
}

int hashmap_remove(hashmap_t *map, const char *key)
{
    if (!map || !map->buckets || map->bucket_count == 0 || !key) {
        return -1;
    }

    uint64_t hash = hashmap_hash_string(key);
    size_t bucket = hash % map->bucket_count;
    hashmap_entry_t *prev = 0;
    hashmap_entry_t *entry = map->buckets[bucket];
    while (entry) {
        if (key_equals(entry, key, hash)) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[bucket] = entry->next;
            }
            entry->key = 0;
            entry->value = 0;
            entry->hash = 0;
            entry->next = 0;
            --map->entry_count;
            return 0;
        }
        prev = entry;
        entry = entry->next;
    }
    return -1;
}

size_t hashmap_size(const hashmap_t *map)
{
    return map ? map->entry_count : 0;
}
