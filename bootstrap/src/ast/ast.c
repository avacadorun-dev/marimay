#include "ruda/ast/ast.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct RudaAstProjector {
    RudaAstTree *tree;
    RudaAstOptions options;
    size_t source_length;
    bool allocation_failed;
    bool limit_reached;
    bool invalid_tree;
} RudaAstProjector;

static bool ruda_ast_token_is_trivia(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_BOM || kind == RUDA_TOKEN_WHITESPACE
        || kind == RUDA_TOKEN_LINE_COMMENT || kind == RUDA_TOKEN_DOC_LINE_COMMENT
        || kind == RUDA_TOKEN_BLOCK_COMMENT || kind == RUDA_TOKEN_DOC_BLOCK_COMMENT;
}

static bool ruda_ast_keeps_token(const RudaSyntaxToken *token) {
    return token != NULL && token->kind != RUDA_TOKEN_EOF
        && !ruda_ast_token_is_trivia(token->kind);
}

static size_t ruda_ast_projected_child_count(const RudaGreenNode *green) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; index < green->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(green, index);
        if (element == NULL) {
            return SIZE_MAX;
        }
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE
            || (element->kind == RUDA_SYNTAX_ELEMENT_TOKEN
                && ruda_ast_keeps_token(&element->as.token))) {
            if (!ruda_size_add(count, 1, &count)) {
                return SIZE_MAX;
            }
        } else if (element->kind != RUDA_SYNTAX_ELEMENT_TOKEN) {
            return SIZE_MAX;
        }
    }
    return count;
}

static void ruda_ast_include_span(
    RudaAstNode *node,
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

static const RudaAstNode *ruda_ast_project_node(
    RudaAstProjector *projector,
    const RudaGreenNode *green,
    size_t depth,
    size_t *cursor
) {
    size_t child_count = 0;
    size_t children_size = 0;
    size_t allocation_size = 0;
    size_t index = 0;
    size_t output_index = 0;
    size_t entry_cursor = *cursor;
    RudaAstNode *node = NULL;
    if (depth > projector->options.max_depth
        || projector->tree->node_count >= projector->options.max_nodes) {
        projector->limit_reached = true;
        return NULL;
    }
    child_count = ruda_ast_projected_child_count(green);
    if (child_count == SIZE_MAX
        || !ruda_size_multiply(child_count, sizeof(RudaAstElement), &children_size)
        || !ruda_size_add(
            offsetof(RudaAstNode, children),
            children_size,
            &allocation_size
        )) {
        projector->invalid_tree = true;
        return NULL;
    }
    node = ruda_arena_allocate(
        &projector->tree->arena,
        allocation_size,
        _Alignof(RudaAstNode)
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
        const RudaSyntaxElement *element = ruda_syntax_child(green, index);
        if (element == NULL) {
            projector->invalid_tree = true;
            return NULL;
        }
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            const RudaAstNode *child = ruda_ast_project_node(
                projector,
                element->as.node,
                depth + 1,
                cursor
            );
            if (child == NULL) {
                return NULL;
            }
            node->children[output_index].kind = RUDA_AST_ELEMENT_NODE;
            node->children[output_index].as.node = child;
            output_index += 1;
            ruda_ast_include_span(
                node,
                child->start,
                child->end,
                child->has_span
            );
        } else if (element->kind == RUDA_SYNTAX_ELEMENT_TOKEN) {
            const RudaSyntaxToken *token = &element->as.token;
            if (token->end < token->start || token->end > projector->source_length
                || (!token->synthetic && token->kind != RUDA_TOKEN_EOF
                    && token->start != *cursor)) {
                projector->invalid_tree = true;
                return NULL;
            }
            if (!token->synthetic && token->kind != RUDA_TOKEN_EOF) {
                *cursor = token->end;
            } else if (token->start != *cursor || token->end != *cursor) {
                projector->invalid_tree = true;
                return NULL;
            }
            if (ruda_ast_keeps_token(token)) {
                node->children[output_index].kind = RUDA_AST_ELEMENT_TOKEN;
                node->children[output_index].as.token = *token;
                output_index += 1;
                ruda_ast_include_span(node, token->start, token->end, true);
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

RudaAstOptions ruda_ast_default_options(void) {
    RudaAstOptions options;
    options.max_nodes = 1000000;
    options.max_depth = 512;
    return options;
}

RudaAstStatus ruda_ast_project(
    const RudaSyntaxTree *syntax,
    RudaAllocator allocator,
    RudaAstOptions options,
    RudaAstTree *result
) {
    RudaAstProjector projector;
    size_t cursor = 0;
    if (result == NULL) {
        return RUDA_AST_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (syntax == NULL || !syntax->initialized || syntax->snapshot == NULL
        || syntax->root == NULL || options.max_nodes == 0
        || options.max_depth == 0 || !ruda_syntax_tree_is_lossless(syntax)) {
        return RUDA_AST_INVALID_ARGUMENT;
    }
    ruda_arena_init(&result->arena, allocator, 8192);
    result->snapshot = syntax->snapshot;
    result->initialized = true;
    memset(&projector, 0, sizeof(projector));
    projector.tree = result;
    projector.options = options;
    projector.source_length = ruda_source_length(syntax->snapshot);
    result->root = ruda_ast_project_node(
        &projector,
        syntax->root,
        1,
        &cursor
    );
    if (projector.limit_reached || projector.allocation_failed
        || projector.invalid_tree || result->root == NULL
        || cursor != projector.source_length) {
        RudaAstStatus status = projector.limit_reached
            ? RUDA_AST_LIMIT_REACHED
            : (projector.allocation_failed
                ? RUDA_AST_ALLOCATION_FAILED
                : RUDA_AST_INVALID_ARGUMENT);
        ruda_ast_tree_dispose(result);
        return status;
    }
    return RUDA_AST_OK;
}

const RudaAstElement *ruda_ast_child(const RudaAstNode *node, size_t index) {
    if (node == NULL || index >= node->child_count) {
        return NULL;
    }
    return &node->children[index];
}

void ruda_ast_tree_dispose(RudaAstTree *tree) {
    if (tree == NULL || !tree->initialized) {
        return;
    }
    ruda_arena_dispose(&tree->arena);
    tree->snapshot = NULL;
    tree->root = NULL;
    tree->node_count = 0;
    tree->initialized = false;
}
