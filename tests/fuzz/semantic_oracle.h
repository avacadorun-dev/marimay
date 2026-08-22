#ifndef MMAY_TESTS_FUZZ_SEMANTIC_ORACLE_H
#define MMAY_TESTS_FUZZ_SEMANTIC_ORACLE_H

#include <stdbool.h>

#include "mmay/ast/ast.h"
#include "mmay/source/source_manager.h"

bool mmay_fuzz_ast_semantically_equal(
    const MarimayAstTree *left,
    const MarimaySourceSnapshot *left_source,
    const MarimayAstTree *right,
    const MarimaySourceSnapshot *right_source
);

#endif
