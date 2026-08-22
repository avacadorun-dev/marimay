#ifndef CHECKER_EXPR_INTERFACES_API_H
#define CHECKER_EXPR_INTERFACES_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool mmay_checker_private_function_ensure_interface_layouts(
    MarimayFunctionChecker *function
);

bool mmay_checker_private_compile_interface_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
