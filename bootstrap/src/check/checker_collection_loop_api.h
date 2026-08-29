#ifndef MMAY_CHECKER_COLLECTION_LOOP_API_H
#define MMAY_CHECKER_COLLECTION_LOOP_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_collection_for(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayExpressionValue *iterable,
    bool *resolved
);

#endif
