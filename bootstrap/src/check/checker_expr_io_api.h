#ifndef RUDA_CHECKER_EXPR_IO_API_H
#define RUDA_CHECKER_EXPR_IO_API_H

#include "checker_function_context.h"

bool ruda_checker_private_compile_io_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);

#endif
