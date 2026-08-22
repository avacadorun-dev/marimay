#ifndef MMAY_CHECKER_EXPR_TRY_API_H
#define MMAY_CHECKER_EXPR_TRY_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_try_expression(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);

#endif
