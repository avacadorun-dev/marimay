#ifndef CHECKER_EQUALITY_API_H
#define CHECKER_EQUALITY_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_equality(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue left,
    MarimayExpressionValue right,
    bool negate,
    MarimayExpressionValue *result
);

#endif
