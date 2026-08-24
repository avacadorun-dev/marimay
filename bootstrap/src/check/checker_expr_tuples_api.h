#ifndef MMAY_CHECKER_EXPR_TUPLES_API_H
#define MMAY_CHECKER_EXPR_TUPLES_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_tuple(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);

bool mmay_checker_private_function_tuple_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRecordId *result
);

bool mmay_function_ensure_tuple_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type
);

#endif
