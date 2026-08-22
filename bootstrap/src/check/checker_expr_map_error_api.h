#ifndef MMAY_CHECKER_EXPR_MAP_ERROR_API_H
#define MMAY_CHECKER_EXPR_MAP_ERROR_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_map_error(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
