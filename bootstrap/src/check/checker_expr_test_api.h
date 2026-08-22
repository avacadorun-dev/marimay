#ifndef MMAY_CHECK_CHECKER_EXPR_TEST_API_H
#define MMAY_CHECK_CHECKER_EXPR_TEST_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool mmay_checker_private_compile_test_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
