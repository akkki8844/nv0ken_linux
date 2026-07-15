#include "heap.h"

#include "lib/math.h"
#include "lib/string.h"

typedef struct block_header {
    size_t size;
    int free;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

static block_header_t *head;
static size_t total_size;
static size_t used_size;

static size_t align_size(size_t size)
{
    return (size + 15u) & ~15u;
}

void heap_init(void *base, size_t size)
{
    head = 0;
    total_size = 0;
    used_size = 0;
    if (!base || size <= sizeof(block_header_t)) {
        return;
    }
    head = base;
    head->size = size - sizeof(block_header_t);
    head->free = 1;
    head->next = 0;
    head->prev = 0;
    total_size = head->size;
}

static void split_block(block_header_t *block, size_t size)
{
    if (block->size < size + sizeof(block_header_t) + 16) {
        return;
    }
    block_header_t *next = (block_header_t *)((char *)(block + 1) + size);
    next->size = block->size - size - sizeof(block_header_t);
    next->free = 1;
    next->next = block->next;
    next->prev = block;
    if (next->next) {
        next->next->prev = next;
    }
    block->next = next;
    block->size = size;
}

void *kmalloc(size_t size)
{
    if (size == 0 || size > (size_t)-16) {
        return 0;
    }
    size = align_size(size);
    for (block_header_t *block = head; block; block = block->next) {
        if (block->free && block->size >= size) {
            split_block(block, size);
            block->free = 0;
            used_size += block->size;
            return block + 1;
        }
    }
    return 0;
}

void *kcalloc(size_t count, size_t size)
{
    if (count != 0 && size > (size_t)-1 / count) {
        return 0;
    }
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

static void coalesce(block_header_t *block)
{
    if (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }
    if (block->prev && block->prev->free) {
        coalesce(block->prev);
    }
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    block_header_t *block = ((block_header_t *)ptr) - 1;
    if ((char *)block < (char *)head ||
        (char *)block >= (char *)head + total_size + sizeof(block_header_t)) {
        return;
    }
    if (!block->free) {
        used_size -= block->size;
    }
    block->free = 1;
    coalesce(block);
}

void *krealloc(void *ptr, size_t size)
{
    if (!ptr) {
        return kmalloc(size);
    }
    if (size == 0) {
        kfree(ptr);
        return 0;
    }
    block_header_t *block = ((block_header_t *)ptr) - 1;
    if ((char *)block < (char *)head ||
        (char *)block >= (char *)head + total_size + sizeof(block_header_t) ||
        block->free) {
        return 0;
    }
    if (block->size >= size) {
        return ptr;
    }
    void *new_ptr = kmalloc(size);
    if (!new_ptr) {
        return 0;
    }
    memcpy(new_ptr, ptr, block->size);
    kfree(ptr);
    return new_ptr;
}

size_t heap_bytes_used(void)
{
    return used_size;
}

size_t heap_bytes_free(void)
{
    return total_size > used_size ? total_size - used_size : 0;
}
