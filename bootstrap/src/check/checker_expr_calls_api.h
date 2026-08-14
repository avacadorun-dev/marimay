#ifndef CHECKER_EXPR_CALLS_API_H
#define CHECKER_EXPR_CALLS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_compile_identity_same(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_ordinary_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
);
#endif
