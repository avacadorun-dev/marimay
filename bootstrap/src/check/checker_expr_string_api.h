#ifndef CHECKER_EXPR_STRING_API_H
#define CHECKER_EXPR_STRING_API_H

#include "checker_function_context.h"

bool mmay_checker_private_emit_string_constant(
    MarimayFunctionChecker *function,
    const unsigned char *bytes,
    size_t length,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_interpolation(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_convert_string_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_convert_parse_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);

#endif
