#ifndef MMAY_AST_AST_H
#define MMAY_AST_AST_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/arena.h"
#include "mmay/syntax/syntax.h"

typedef enum MarimayAstStatus {
    MMAY_AST_OK,
    MMAY_AST_INVALID_ARGUMENT,
    MMAY_AST_ALLOCATION_FAILED,
    MMAY_AST_LIMIT_REACHED
} MarimayAstStatus;

typedef struct MarimayAstOptions {
    size_t max_nodes;
    size_t max_depth;
} MarimayAstOptions;

typedef enum MarimayAstElementKind {
    MMAY_AST_ELEMENT_NODE,
    MMAY_AST_ELEMENT_TOKEN
} MarimayAstElementKind;

typedef struct MarimayAstNode MarimayAstNode;

typedef struct MarimayAstElement {
    MarimayAstElementKind kind;
    union {
        const MarimayAstNode *node;
        MarimaySyntaxToken token;
    } as;
} MarimayAstElement;

struct MarimayAstNode {
    MarimaySyntaxKind kind;
    size_t start;
    size_t end;
    size_t child_count;
    bool has_span;
    MarimayAstElement children[];
};

typedef struct MarimayAstTree {
    MarimayArena arena;
    const MarimaySourceSnapshot *snapshot;
    const MarimayAstNode *root;
    size_t node_count;
    bool initialized;
} MarimayAstTree;

MarimayAstOptions mmay_ast_default_options(void);
bool mmay_ast_options_valid(MarimayAstOptions options);

/*
 * Projects a lossless green tree into an immutable trivia-free structural AST.
 * The source snapshot remains owned by the source manager. The result owns all
 * AST nodes until disposal and is failure-atomic on allocation or limit errors.
 */
MarimayAstStatus mmay_ast_project(
    const MarimaySyntaxTree *syntax,
    MarimayAllocator allocator,
    MarimayAstOptions options,
    MarimayAstTree *result
);

const MarimayAstElement *mmay_ast_child(const MarimayAstNode *node, size_t index);
void mmay_ast_tree_dispose(MarimayAstTree *tree);

#endif
