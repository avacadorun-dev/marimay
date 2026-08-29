#ifndef MMAY_CHECKER_EXPR_FS_API_H
#define MMAY_CHECKER_EXPR_FS_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_fs_primitive_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
