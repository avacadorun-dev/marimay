#ifndef MMAY_CHECK_CHECKER_DEFER_API_H
#define MMAY_CHECK_CHECKER_DEFER_API_H

#include "checker_function_context.h"

bool mmay_checker_private_compile_defer(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
);
bool mmay_checker_private_emit_defers_from(
    MarimayFunctionChecker *function,
    size_t first
);
bool mmay_checker_private_emit_defers_deeper_than(
    MarimayFunctionChecker *function,
    size_t scope_depth
);

#endif
