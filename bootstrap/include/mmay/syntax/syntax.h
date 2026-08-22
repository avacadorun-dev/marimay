#ifndef MMAY_SYNTAX_SYNTAX_H
#define MMAY_SYNTAX_SYNTAX_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/arena.h"
#include "mmay/generated/syntax_kinds.h"
#include "mmay/lex/token.h"
#include "mmay/source/source_manager.h"

typedef struct MarimaySyntaxToken {
    MarimayTokenKind kind;
    size_t start;
    size_t end;
    bool synthetic;
} MarimaySyntaxToken;

typedef enum MarimaySyntaxElementKind {
    MMAY_SYNTAX_ELEMENT_NODE,
    MMAY_SYNTAX_ELEMENT_TOKEN
} MarimaySyntaxElementKind;

typedef struct MarimayGreenNode MarimayGreenNode;
typedef struct MarimaySyntaxStorage MarimaySyntaxStorage;

typedef struct MarimaySyntaxElement {
    MarimaySyntaxElementKind kind;
    union {
        const MarimayGreenNode *node;
        MarimaySyntaxToken token;
    } as;
} MarimaySyntaxElement;

struct MarimayGreenNode {
    MarimaySyntaxKind kind;
    size_t child_count;
    size_t text_length;
    MarimaySyntaxElement children[];
};

typedef struct MarimaySyntaxTree {
    MarimaySyntaxStorage *storage;
    const MarimaySourceSnapshot *snapshot;
    const MarimayGreenNode *root;
    bool initialized;
} MarimaySyntaxTree;

/* Internal green-node storage hooks used by the bounded parser builders. */
bool mmay_syntax_tree_storage_init(
    MarimaySyntaxTree *tree,
    MarimayAllocator allocator
);
void *mmay_syntax_tree_allocate(
    MarimaySyntaxTree *tree,
    size_t size,
    size_t alignment
);
bool mmay_syntax_tree_retain_storage(
    MarimaySyntaxTree *tree,
    const MarimaySyntaxTree *retained
);

const char *mmay_syntax_kind_name(MarimaySyntaxKind kind);
const MarimaySyntaxElement *mmay_syntax_child(const MarimayGreenNode *node, size_t index);

/* Every non-synthetic token recursively reconstructs the source exactly. */
bool mmay_syntax_tree_is_lossless(const MarimaySyntaxTree *tree);

void mmay_syntax_tree_dispose(MarimaySyntaxTree *tree);

#endif
