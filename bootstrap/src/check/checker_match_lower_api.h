#ifndef CHECKER_MATCH_LOWER_API_H
#define CHECKER_MATCH_LOWER_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool mmay_checker_private_compile_match_arm_body(
    MarimayFunctionChecker *function,
    const MarimayAstNode *body,
    bool body_is_block,
    MarimayMirBlockId *join,
    MarimayTypeId *match_type,
    MarimayMirRegister *target
);
 bool mmay_checker_private_compile_match_guard(
    MarimayFunctionChecker *function,
    const MarimayAstNode *guard,
    MarimayExpressionValue *result,
    bool *never
);
 bool mmay_checker_private_compile_bool_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_function_emit_string_constant(
    MarimayFunctionChecker *function,
    MarimayMirStringId string_constant,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
#endif
