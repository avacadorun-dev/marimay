#ifndef MMAY_CHECKER_ARRAY_SEARCH_API_H
#define MMAY_CHECKER_ARRAY_SEARCH_API_H
#include "checker_function_context.h"

/* D099 after exact receiver-directed selection; receiver is already evaluated. */
bool mmay_checker_private_compile_array_search_call(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayExpressionValue *receiver, bool *resolved,
    MarimayExpressionValue *result);
#endif
