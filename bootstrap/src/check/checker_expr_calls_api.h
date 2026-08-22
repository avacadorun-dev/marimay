#ifndef CHECKER_EXPR_CALLS_API_H
#define CHECKER_EXPR_CALLS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_compile_identity_same(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool *resolved,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_ordinary_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_ordinary_call_with_receiver(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *receiver_node,
    const MarimayExpressionValue *receiver,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
#endif
