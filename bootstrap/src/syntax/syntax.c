#include "ruda/syntax/syntax.h"

#include <stdint.h>

struct RudaSyntaxStorage {
    RudaAllocator allocator;
    RudaArena arena;
    RudaSyntaxStorage *retained;
    size_t references;
};

#include "../generated/syntax_data.inc"

const char *ruda_syntax_kind_name(RudaSyntaxKind kind) {
    size_t index = (size_t)kind;
    if (index >= sizeof(ruda_syntax_names) / sizeof(ruda_syntax_names[0])) {
        return "UNKNOWN";
    }
    return ruda_syntax_names[index];
}

const RudaSyntaxElement *ruda_syntax_child(const RudaGreenNode *node, size_t index) {
    if (node == NULL || index >= node->child_count) {
        return NULL;
    }
    return &node->children[index];
}

bool ruda_syntax_tree_storage_init(
    RudaSyntaxTree *tree,
    RudaAllocator allocator
) {
    RudaSyntaxStorage *storage = NULL;
    if (tree == NULL || tree->storage != NULL) {
        return false;
    }
    storage = ruda_allocate(&allocator, sizeof(*storage));
    if (storage == NULL) {
        return false;
    }
    storage->allocator = allocator;
    storage->retained = NULL;
    storage->references = 1;
    ruda_arena_init(&storage->arena, allocator, 8192);
    tree->storage = storage;
    return true;
}

void *ruda_syntax_tree_allocate(
    RudaSyntaxTree *tree,
    size_t size,
    size_t alignment
) {
    if (tree == NULL || tree->storage == NULL) {
        return NULL;
    }
    return ruda_arena_allocate(&tree->storage->arena, size, alignment);
}

bool ruda_syntax_tree_retain_storage(
    RudaSyntaxTree *tree,
    const RudaSyntaxTree *retained
) {
    if (tree == NULL || tree->storage == NULL
        || tree->storage->retained != NULL || retained == NULL
        || retained->storage == NULL || tree->storage == retained->storage
        || retained->storage->references == SIZE_MAX) {
        return false;
    }
    retained->storage->references += 1;
    tree->storage->retained = retained->storage;
    return true;
}

static bool ruda_syntax_node_is_lossless(
    const RudaGreenNode *node,
    size_t source_length,
    size_t *cursor
) {
    size_t index = 0;
    size_t node_length = 0;
    if (node == NULL) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = &node->children[index];
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            size_t before = *cursor;
            size_t child_length = 0;
            if (!ruda_syntax_node_is_lossless(element->as.node, source_length, cursor)) {
                return false;
            }
            child_length = *cursor - before;
            if (!ruda_size_add(node_length, child_length, &node_length)) {
                return false;
            }
        } else if (element->kind == RUDA_SYNTAX_ELEMENT_TOKEN) {
            const RudaSyntaxToken *token = &element->as.token;
            if (token->end < token->start || token->end > source_length) {
                return false;
            }
            if (token->synthetic) {
                if (token->start != token->end || token->start != *cursor) {
                    return false;
                }
            } else if (token->kind == RUDA_TOKEN_EOF) {
                if (token->start != token->end || token->start != *cursor) {
                    return false;
                }
            } else {
                if (token->start != *cursor) {
                    return false;
                }
                *cursor = token->end;
                if (!ruda_size_add(
                        node_length,
                        token->end - token->start,
                        &node_length
                    )) {
                    return false;
                }
            }
        } else {
            return false;
        }
    }
    return node_length == node->text_length;
}

bool ruda_syntax_tree_is_lossless(const RudaSyntaxTree *tree) {
    size_t cursor = 0;
    size_t source_length = 0;
    if (tree == NULL || !tree->initialized || tree->snapshot == NULL
        || tree->root == NULL) {
        return false;
    }
    source_length = ruda_source_length(tree->snapshot);
    return ruda_syntax_node_is_lossless(tree->root, source_length, &cursor)
        && cursor == source_length
        && tree->root->text_length == source_length;
}

void ruda_syntax_tree_dispose(RudaSyntaxTree *tree) {
    RudaSyntaxStorage *storage = NULL;
    if (tree == NULL || !tree->initialized) {
        return;
    }
    storage = tree->storage;
    while (storage != NULL) {
        RudaSyntaxStorage *retained = NULL;
        RudaAllocator allocator;
        if (storage->references > 1) {
            storage->references -= 1;
            break;
        }
        retained = storage->retained;
        allocator = storage->allocator;
        ruda_arena_dispose(&storage->arena);
        ruda_deallocate(&allocator, storage, sizeof(*storage));
        storage = retained;
    }
    tree->storage = NULL;
    tree->snapshot = NULL;
    tree->root = NULL;
    tree->initialized = false;
}
