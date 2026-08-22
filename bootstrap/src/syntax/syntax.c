#include "mmay/syntax/syntax.h"

#include <stdint.h>

struct MarimaySyntaxStorage {
    MarimayAllocator allocator;
    MarimayArena arena;
    MarimaySyntaxStorage *retained;
    size_t references;
};

#include "../generated/syntax_data.inc"

const char *mmay_syntax_kind_name(MarimaySyntaxKind kind) {
    size_t index = (size_t)kind;
    if (index >= sizeof(mmay_syntax_names) / sizeof(mmay_syntax_names[0])) {
        return "UNKNOWN";
    }
    return mmay_syntax_names[index];
}

const MarimaySyntaxElement *mmay_syntax_child(const MarimayGreenNode *node, size_t index) {
    if (node == NULL || index >= node->child_count) {
        return NULL;
    }
    return &node->children[index];
}

bool mmay_syntax_tree_storage_init(
    MarimaySyntaxTree *tree,
    MarimayAllocator allocator
) {
    MarimaySyntaxStorage *storage = NULL;
    if (tree == NULL || tree->storage != NULL) {
        return false;
    }
    storage = mmay_allocate(&allocator, sizeof(*storage));
    if (storage == NULL) {
        return false;
    }
    storage->allocator = allocator;
    storage->retained = NULL;
    storage->references = 1;
    mmay_arena_init(&storage->arena, allocator, 8192);
    tree->storage = storage;
    return true;
}

void *mmay_syntax_tree_allocate(
    MarimaySyntaxTree *tree,
    size_t size,
    size_t alignment
) {
    if (tree == NULL || tree->storage == NULL) {
        return NULL;
    }
    return mmay_arena_allocate(&tree->storage->arena, size, alignment);
}

bool mmay_syntax_tree_retain_storage(
    MarimaySyntaxTree *tree,
    const MarimaySyntaxTree *retained
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

static bool mmay_syntax_node_is_lossless(
    const MarimayGreenNode *node,
    size_t source_length,
    size_t *cursor
) {
    size_t index = 0;
    size_t node_length = 0;
    if (node == NULL) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = &node->children[index];
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            size_t before = *cursor;
            size_t child_length = 0;
            if (!mmay_syntax_node_is_lossless(element->as.node, source_length, cursor)) {
                return false;
            }
            child_length = *cursor - before;
            if (!mmay_size_add(node_length, child_length, &node_length)) {
                return false;
            }
        } else if (element->kind == MMAY_SYNTAX_ELEMENT_TOKEN) {
            const MarimaySyntaxToken *token = &element->as.token;
            if (token->end < token->start || token->end > source_length) {
                return false;
            }
            if (token->synthetic) {
                if (token->start != token->end || token->start != *cursor) {
                    return false;
                }
            } else if (token->kind == MMAY_TOKEN_EOF) {
                if (token->start != token->end || token->start != *cursor) {
                    return false;
                }
            } else {
                if (token->start != *cursor) {
                    return false;
                }
                *cursor = token->end;
                if (!mmay_size_add(
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

bool mmay_syntax_tree_is_lossless(const MarimaySyntaxTree *tree) {
    size_t cursor = 0;
    size_t source_length = 0;
    if (tree == NULL || !tree->initialized || tree->snapshot == NULL
        || tree->root == NULL) {
        return false;
    }
    source_length = mmay_source_length(tree->snapshot);
    return mmay_syntax_node_is_lossless(tree->root, source_length, &cursor)
        && cursor == source_length
        && tree->root->text_length == source_length;
}

void mmay_syntax_tree_dispose(MarimaySyntaxTree *tree) {
    MarimaySyntaxStorage *storage = NULL;
    if (tree == NULL || !tree->initialized) {
        return;
    }
    storage = tree->storage;
    while (storage != NULL) {
        MarimaySyntaxStorage *retained = NULL;
        MarimayAllocator allocator;
        if (storage->references > 1) {
            storage->references -= 1;
            break;
        }
        retained = storage->retained;
        allocator = storage->allocator;
        mmay_arena_dispose(&storage->arena);
        mmay_deallocate(&allocator, storage, sizeof(*storage));
        storage = retained;
    }
    tree->storage = NULL;
    tree->snapshot = NULL;
    tree->root = NULL;
    tree->initialized = false;
}
