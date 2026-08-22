#ifndef CHECKER_EXPRESSION_API_H
#define CHECKER_EXPRESSION_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool mmay_checker_private_compile_expression_raw(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
bool mmay_checker_private_maybe_box_interface(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
#endif
