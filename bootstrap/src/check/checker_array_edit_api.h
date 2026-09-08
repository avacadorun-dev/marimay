#ifndef MMAY_CHECKER_ARRAY_EDIT_API_H
#define MMAY_CHECKER_ARRAY_EDIT_API_H

#include "checker_function_context.h"

/* D097 after ordinary receiver-directed name/type selection. */
bool mmay_checker_private_compile_array_edit_call(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayExpressionValue *receiver, bool *resolved,
    MarimayExpressionValue *result
);

#endif
