#ifndef MMAY_CHECKER_EXPR_MAP_ERROR_API_H
#define MMAY_CHECKER_EXPR_MAP_ERROR_API_H

#include "checker_function_context.h"

/* Returns only the enclosing Result's error type for a direct mapError
 * operand of '?', through parentheses, and its borrowed normalized AST node.
 * Allocation-free; an ineligible operand produces INVALID and NULL. */
MarimayTypeId mmay_checker_private_map_error_operand_context(
    MarimayFunctionChecker *function,
    const MarimayAstNode *operand,
    const MarimayAstNode **target
);

bool mmay_checker_private_compile_map_error(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
