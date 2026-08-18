#ifndef RUDA_CHECK_CHECKER_EXPR_TEST_API_H
#define RUDA_CHECK_CHECKER_EXPR_TEST_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

bool ruda_checker_private_compile_test_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);

#endif
