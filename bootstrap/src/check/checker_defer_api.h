#ifndef RUDA_CHECK_CHECKER_DEFER_API_H
#define RUDA_CHECK_CHECKER_DEFER_API_H

#include "checker_function_context.h"

bool ruda_checker_private_compile_defer(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
);
bool ruda_checker_private_emit_defers_from(
    RudaFunctionChecker *function,
    size_t first
);
bool ruda_checker_private_emit_defers_deeper_than(
    RudaFunctionChecker *function,
    size_t scope_depth
);

#endif
