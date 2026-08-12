#ifndef RUDA_TESTS_FUZZ_SEMANTIC_ORACLE_H
#define RUDA_TESTS_FUZZ_SEMANTIC_ORACLE_H

#include <stdbool.h>

#include "ruda/ast/ast.h"
#include "ruda/source/source_manager.h"

bool ruda_fuzz_ast_semantically_equal(
    const RudaAstTree *left,
    const RudaSourceSnapshot *left_source,
    const RudaAstTree *right,
    const RudaSourceSnapshot *right_source
);

#endif
