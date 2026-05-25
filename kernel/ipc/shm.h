#ifndef NV0KEN_IPC_SHM_H
#define NV0KEN_IPC_SHM_H

#include <stddef.h>
#include <stdint.h>

#define SHM_NAME_MAX 32

typedef struct {
    uint32_t id;
    char name[SHM_NAME_MAX];
    void *base;
    size_t size;
    uint32_t refs;
} shm_segment_t;

void shm_init(void);
shm_segment_t *shm_create(const char *name, size_t size);
shm_segment_t *shm_find(const char *name);
void *shm_attach(shm_segment_t *segment);
void shm_detach(shm_segment_t *segment);

#endif
