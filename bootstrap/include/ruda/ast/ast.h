#ifndef RUDA_AST_AST_H
#define RUDA_AST_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/arena.h"
#include "ruda/syntax/syntax.h"

typedef enum RudaAstStatus {
    RUDA_AST_OK,
    RUDA_AST_INVALID_ARGUMENT,
    RUDA_AST_ALLOCATION_FAILED,
    RUDA_AST_LIMIT_REACHED
} RudaAstStatus;

typedef struct RudaAstOptions {
    size_t max_nodes;
    size_t max_depth;
} RudaAstOptions;

typedef enum RudaAstElementKind {
    RUDA_AST_ELEMENT_NODE,
    RUDA_AST_ELEMENT_TOKEN
} RudaAstElementKind;

typedef struct RudaAstNode RudaAstNode;

typedef struct RudaAstElement {
    RudaAstElementKind kind;
    union {
        const RudaAstNode *node;
        RudaSyntaxToken token;
    } as;
} RudaAstElement;

struct RudaAstNode {
    RudaSyntaxKind kind;
    size_t start;
    size_t end;
    size_t child_count;
    bool has_span;
    RudaAstElement children[];
};

typedef struct RudaAstTree {
    RudaArena arena;
    const RudaSourceSnapshot *snapshot;
    const RudaAstNode *root;
    size_t node_count;
    bool initialized;
} RudaAstTree;

RudaAstOptions ruda_ast_default_options(void);

/*
 * Projects a lossless green tree into an immutable trivia-free structural AST.
 * The source snapshot remains owned by the source manager. The result owns all
 * AST nodes until disposal and is failure-atomic on allocation or limit errors.
 */
RudaAstStatus ruda_ast_project(
    const RudaSyntaxTree *syntax,
    RudaAllocator allocator,
    RudaAstOptions options,
    RudaAstTree *result
);

const RudaAstElement *ruda_ast_child(const RudaAstNode *node, size_t index);
void ruda_ast_tree_dispose(RudaAstTree *tree);

#endif
