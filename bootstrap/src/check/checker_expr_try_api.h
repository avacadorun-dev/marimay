#ifndef RUDA_CHECKER_EXPR_TRY_API_H
#define RUDA_CHECKER_EXPR_TRY_API_H

#include "checker_function_context.h"

bool ruda_checker_private_compile_try_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);

#endif
