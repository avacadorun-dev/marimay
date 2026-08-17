#ifndef RUDA_CHECKER_EXPR_PRELUDE_API_H
#define RUDA_CHECKER_EXPR_PRELUDE_API_H

#include "checker_function_context.h"

bool ruda_checker_private_compile_prelude_constructor(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);

#endif
