#ifndef CHECKER_EXPR_NUMERIC_API_H
#define CHECKER_EXPR_NUMERIC_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool ruda_checker_private_compile_numeric_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);
bool ruda_checker_private_compile_integer_conversion(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
bool ruda_checker_private_compile_convert_format_call(
    RudaFunctionChecker *function, const RudaAstNode *node,
    RudaTypeId expected, bool *resolved, RudaExpressionValue *result);

#endif
