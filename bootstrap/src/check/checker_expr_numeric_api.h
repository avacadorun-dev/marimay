#ifndef CHECKER_EXPR_NUMERIC_API_H
#define CHECKER_EXPR_NUMERIC_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool mmay_checker_private_compile_numeric_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_integer_conversion(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_convert_format_call(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, bool *resolved, MarimayExpressionValue *result);

#endif
