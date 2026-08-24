#ifndef MMAY_CHECKER_TUPLE_MATCH_API_H
#define MMAY_CHECKER_TUPLE_MATCH_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_tuple_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayExpressionValue *result
);

#endif
