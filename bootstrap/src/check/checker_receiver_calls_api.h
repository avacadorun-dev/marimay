#ifndef CHECKER_RECEIVER_CALLS_API_H
#define CHECKER_RECEIVER_CALLS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool mmay_checker_private_compile_receiver_collection_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
