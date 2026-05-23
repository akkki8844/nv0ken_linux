#ifndef NV0KEN_LIB_LIST_H
#define NV0KEN_LIB_LIST_H

#include <stddef.h>

typedef struct list_node {
    struct list_node *prev;
    struct list_node *next;
} list_node_t;

#define LIST_INIT(name) { &(name), &(name) }
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

static inline void list_init(list_node_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline int list_empty(const list_node_t *list)
{
    return list->next == list;
}

static inline void list_insert_after(list_node_t *pos, list_node_t *node)
{
    node->next = pos->next;
    node->prev = pos;
    pos->next->prev = node;
    pos->next = node;
}

static inline void list_insert_before(list_node_t *pos, list_node_t *node)
{
    node->next = pos;
    node->prev = pos->prev;
    pos->prev->next = node;
    pos->prev = node;
}

static inline void list_push_front(list_node_t *list, list_node_t *node)
{
    list_insert_after(list, node);
}

static inline void list_push_back(list_node_t *list, list_node_t *node)
{
    list_insert_before(list, node);
}

static inline void list_remove(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

#define list_for_each(pos, list) \
    for ((pos) = (list)->next; (pos) != (list); (pos) = (pos)->next)

#define list_for_each_safe(pos, tmp, list) \
    for ((pos) = (list)->next, (tmp) = (pos)->next; \
         (pos) != (list); \
         (pos) = (tmp), (tmp) = (pos)->next)

#endif
