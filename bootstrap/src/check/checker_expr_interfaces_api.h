#ifndef CHECKER_EXPR_INTERFACES_API_H
#define CHECKER_EXPR_INTERFACES_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool ruda_checker_private_function_ensure_interface_layouts(
    RudaFunctionChecker *function
);

bool ruda_checker_private_compile_interface_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
);

#endif
