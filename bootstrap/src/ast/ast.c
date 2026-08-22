#include "mmay/ast/ast.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct MarimayAstProjector {
    MarimayAstTree *tree;
    MarimayAstOptions options;
    size_t source_length;
    bool allocation_failed;
    bool limit_reached;
    bool invalid_tree;
} MarimayAstProjector;

static bool mmay_ast_token_is_trivia(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_BOM || kind == MMAY_TOKEN_WHITESPACE
        || kind == MMAY_TOKEN_LINE_COMMENT || kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || kind == MMAY_TOKEN_BLOCK_COMMENT || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT;
}

static bool mmay_ast_keeps_token(const MarimaySyntaxToken *token) {
    return token != NULL && token->kind != MMAY_TOKEN_EOF
        && !mmay_ast_token_is_trivia(token->kind);
}

static size_t mmay_ast_projected_child_count(const MarimayGreenNode *green) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; index < green->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(green, index);
        if (element == NULL) {
            return SIZE_MAX;
        }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE
            || (element->kind == MMAY_SYNTAX_ELEMENT_TOKEN
                && mmay_ast_keeps_token(&element->as.token))) {
            if (!mmay_size_add(count, 1, &count)) {
                return SIZE_MAX;
            }
        } else if (element->kind != MMAY_SYNTAX_ELEMENT_TOKEN) {
            return SIZE_MAX;
        }
    }
    return count;
}

static void mmay_ast_include_span(
    MarimayAstNode *node,
    size_t start,
    size_t end,
    bool has_span
) {
    if (!has_span) {
        return;
    }
    if (!node->has_span) {
        node->start = start;
        node->end = end;
        node->has_span = true;
        return;
    }
    if (start < node->start) {
        node->start = start;
    }
    if (end > node->end) {
        node->end = end;
    }
}

static const MarimayAstNode *mmay_ast_project_node(
    MarimayAstProjector *projector,
    const MarimayGreenNode *green,
    size_t depth,
    size_t *cursor
) {
    size_t child_count = 0;
    size_t children_size = 0;
    size_t allocation_size = 0;
    size_t index = 0;
    size_t output_index = 0;
    size_t entry_cursor = *cursor;
    MarimayAstNode *node = NULL;
    if (depth > projector->options.max_depth
        || projector->tree->node_count >= projector->options.max_nodes) {
        projector->limit_reached = true;
        return NULL;
    }
    child_count = mmay_ast_projected_child_count(green);
    if (child_count == SIZE_MAX
        || !mmay_size_multiply(child_count, sizeof(MarimayAstElement), &children_size)
        || !mmay_size_add(
            offsetof(MarimayAstNode, children),
            children_size,
            &allocation_size
        )) {
        projector->invalid_tree = true;
        return NULL;
    }
    node = mmay_arena_allocate(
        &projector->tree->arena,
        allocation_size,
        _Alignof(MarimayAstNode)
    );
    if (node == NULL) {
        projector->allocation_failed = true;
        return NULL;
    }
    projector->tree->node_count += 1;
    node->kind = green->kind;
    node->start = entry_cursor;
    node->end = entry_cursor;
    node->child_count = child_count;
    node->has_span = false;
    for (index = 0; index < green->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(green, index);
        if (element == NULL) {
            projector->invalid_tree = true;
            return NULL;
        }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            const MarimayAstNode *child = mmay_ast_project_node(
                projector,
                element->as.node,
                depth + 1,
                cursor
            );
            if (child == NULL) {
                return NULL;
            }
            node->children[output_index].kind = MMAY_AST_ELEMENT_NODE;
            node->children[output_index].as.node = child;
            output_index += 1;
            mmay_ast_include_span(
                node,
                child->start,
                child->end,
                child->has_span
            );
        } else if (element->kind == MMAY_SYNTAX_ELEMENT_TOKEN) {
            const MarimaySyntaxToken *token = &element->as.token;
            if (token->end < token->start || token->end > projector->source_length
                || (!token->synthetic && token->kind != MMAY_TOKEN_EOF
                    && token->start != *cursor)) {
                projector->invalid_tree = true;
                return NULL;
            }
            if (!token->synthetic && token->kind != MMAY_TOKEN_EOF) {
                *cursor = token->end;
            } else if (token->start != *cursor || token->end != *cursor) {
                projector->invalid_tree = true;
                return NULL;
            }
            if (mmay_ast_keeps_token(token)) {
                node->children[output_index].kind = MMAY_AST_ELEMENT_TOKEN;
                node->children[output_index].as.token = *token;
                output_index += 1;
                mmay_ast_include_span(node, token->start, token->end, true);
            }
        } else {
            projector->invalid_tree = true;
            return NULL;
        }
    }
    if (output_index != child_count) {
        projector->invalid_tree = true;
        return NULL;
    }
    return node;
}

MarimayAstOptions mmay_ast_default_options(void) {
    MarimayAstOptions options;
    options.max_nodes = 1000000;
    options.max_depth = 512;
    return options;
}

bool mmay_ast_options_valid(MarimayAstOptions options) {
    return options.max_nodes != 0 && options.max_depth != 0;
}

MarimayAstStatus mmay_ast_project(
    const MarimaySyntaxTree *syntax,
    MarimayAllocator allocator,
    MarimayAstOptions options,
    MarimayAstTree *result
) {
    MarimayAstProjector projector;
    size_t cursor = 0;
    if (result == NULL) {
        return MMAY_AST_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (syntax == NULL || !syntax->initialized || syntax->snapshot == NULL
        || syntax->root == NULL || !mmay_ast_options_valid(options)
        || !mmay_syntax_tree_is_lossless(syntax)) {
        return MMAY_AST_INVALID_ARGUMENT;
    }
    mmay_arena_init(&result->arena, allocator, 8192);
    result->snapshot = syntax->snapshot;
    result->initialized = true;
    memset(&projector, 0, sizeof(projector));
    projector.tree = result;
    projector.options = options;
    projector.source_length = mmay_source_length(syntax->snapshot);
    result->root = mmay_ast_project_node(
        &projector,
        syntax->root,
        1,
        &cursor
    );
    if (projector.limit_reached || projector.allocation_failed
        || projector.invalid_tree || result->root == NULL
        || cursor != projector.source_length) {
        MarimayAstStatus status = projector.limit_reached
            ? MMAY_AST_LIMIT_REACHED
            : (projector.allocation_failed
                ? MMAY_AST_ALLOCATION_FAILED
                : MMAY_AST_INVALID_ARGUMENT);
        mmay_ast_tree_dispose(result);
        return status;
    }
    return MMAY_AST_OK;
}

const MarimayAstElement *mmay_ast_child(const MarimayAstNode *node, size_t index) {
    if (node == NULL || index >= node->child_count) {
        return NULL;
    }
    return &node->children[index];
}

void mmay_ast_tree_dispose(MarimayAstTree *tree) {
    if (tree == NULL || !tree->initialized) {
        return;
    }
    mmay_arena_dispose(&tree->arena);
    tree->snapshot = NULL;
    tree->root = NULL;
    tree->node_count = 0;
    tree->initialized = false;
}
