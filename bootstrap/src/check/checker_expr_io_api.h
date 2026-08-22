#ifndef MMAY_CHECKER_EXPR_IO_API_H
#define MMAY_CHECKER_EXPR_IO_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_io_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
