#ifndef CHECKER_EXPRESSION_API_H
#define CHECKER_EXPRESSION_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool ruda_checker_private_compile_expression_raw(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
bool ruda_checker_private_maybe_box_interface(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
#endif
