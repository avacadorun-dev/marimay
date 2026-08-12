#ifndef RUDA_SYNTAX_SYNTAX_H
#define RUDA_SYNTAX_SYNTAX_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/arena.h"
#include "ruda/generated/syntax_kinds.h"
#include "ruda/lex/token.h"
#include "ruda/source/source_manager.h"

typedef struct RudaSyntaxToken {
    RudaTokenKind kind;
    size_t start;
    size_t end;
    bool synthetic;
} RudaSyntaxToken;

typedef enum RudaSyntaxElementKind {
    RUDA_SYNTAX_ELEMENT_NODE,
    RUDA_SYNTAX_ELEMENT_TOKEN
} RudaSyntaxElementKind;

typedef struct RudaGreenNode RudaGreenNode;
typedef struct RudaSyntaxStorage RudaSyntaxStorage;

typedef struct RudaSyntaxElement {
    RudaSyntaxElementKind kind;
    union {
        const RudaGreenNode *node;
        RudaSyntaxToken token;
    } as;
} RudaSyntaxElement;

struct RudaGreenNode {
    RudaSyntaxKind kind;
    size_t child_count;
    size_t text_length;
    RudaSyntaxElement children[];
};

typedef struct RudaSyntaxTree {
    RudaSyntaxStorage *storage;
    const RudaSourceSnapshot *snapshot;
    const RudaGreenNode *root;
    bool initialized;
} RudaSyntaxTree;

/* Internal green-node storage hooks used by the bounded parser builders. */
bool ruda_syntax_tree_storage_init(
    RudaSyntaxTree *tree,
    RudaAllocator allocator
);
void *ruda_syntax_tree_allocate(
    RudaSyntaxTree *tree,
    size_t size,
    size_t alignment
);
bool ruda_syntax_tree_retain_storage(
    RudaSyntaxTree *tree,
    const RudaSyntaxTree *retained
);

const char *ruda_syntax_kind_name(RudaSyntaxKind kind);
const RudaSyntaxElement *ruda_syntax_child(const RudaGreenNode *node, size_t index);

/* Every non-synthetic token recursively reconstructs the source exactly. */
bool ruda_syntax_tree_is_lossless(const RudaSyntaxTree *tree);

void ruda_syntax_tree_dispose(RudaSyntaxTree *tree);

#endif
