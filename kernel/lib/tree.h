#ifndef NV0KEN_LIB_TREE_H
#define NV0KEN_LIB_TREE_H

typedef struct tree_node {
    struct tree_node *parent;
    struct tree_node *first_child;
    struct tree_node *last_child;
    struct tree_node *prev_sibling;
    struct tree_node *next_sibling;
} tree_node_t;

static inline void tree_node_init(tree_node_t *node)
{
    node->parent = 0;
    node->first_child = 0;
    node->last_child = 0;
    node->prev_sibling = 0;
    node->next_sibling = 0;
}

static inline int tree_node_attached(const tree_node_t *node)
{
    return node && node->parent != 0;
}

static inline void tree_append_child(tree_node_t *parent, tree_node_t *child)
{
    child->parent = parent;
    child->prev_sibling = parent->last_child;
    child->next_sibling = 0;
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
}

static inline void tree_remove(tree_node_t *node)
{
    if (!node || !node->parent) {
        return;
    }
    if (node->prev_sibling) {
        node->prev_sibling->next_sibling = node->next_sibling;
    } else {
        node->parent->first_child = node->next_sibling;
    }
    if (node->next_sibling) {
        node->next_sibling->prev_sibling = node->prev_sibling;
    } else {
        node->parent->last_child = node->prev_sibling;
    }
    tree_node_init(node);
}

#endif
