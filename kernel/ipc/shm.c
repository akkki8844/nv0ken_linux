#include "shm.h"

#include "../lib/string.h"
#include "../mm/heap.h"

#define SHM_MAX_SEGMENTS 64

static shm_segment_t segments[SHM_MAX_SEGMENTS];
static uint32_t next_id = 1;

void shm_init(void)
{
    memset(segments, 0, sizeof(segments));
    next_id = 1;
}

shm_segment_t *shm_create(const char *name, size_t size)
{
    if (!name || size == 0) {
        return 0;
    }

    shm_segment_t *existing = shm_find(name);
    if (existing) {
        ++existing->refs;
        return existing;
    }

    for (size_t index = 0; index < SHM_MAX_SEGMENTS; ++index) {
        if (segments[index].id != 0) {
            continue;
        }

        void *base = kcalloc(1, size);
        if (!base) {
            return 0;
        }

        segments[index].id = next_id++;
        segments[index].base = base;
        segments[index].size = size;
        segments[index].refs = 1;
        size_t length = strlen(name);
        if (length >= SHM_NAME_MAX) {
            length = SHM_NAME_MAX - 1;
        }
        memcpy(segments[index].name, name, length);
        segments[index].name[length] = '\0';
        return &segments[index];
    }

    return 0;
}

shm_segment_t *shm_find(const char *name)
{
    if (!name) {
        return 0;
    }

    for (size_t index = 0; index < SHM_MAX_SEGMENTS; ++index) {
        if (segments[index].id && strcmp(segments[index].name, name) == 0) {
            return &segments[index];
        }
    }
    return 0;
}

void *shm_attach(shm_segment_t *segment)
{
    if (!segment || !segment->id) {
        return 0;
    }

    ++segment->refs;
    return segment->base;
}

void shm_detach(shm_segment_t *segment)
{
    if (!segment || !segment->id || segment->refs == 0) {
        return;
    }

    if (--segment->refs == 0) {
        kfree(segment->base);
        memset(segment, 0, sizeof(*segment));
    }
}
