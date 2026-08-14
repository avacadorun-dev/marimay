#ifndef CHECKER_MATCH_LOWER_API_H
#define CHECKER_MATCH_LOWER_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool ruda_checker_private_compile_yield(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
);
 bool ruda_checker_private_compile_match_arm_body(
    RudaFunctionChecker *function,
    const RudaAstNode *body,
    bool body_is_block,
    RudaMirBlockId *join,
    RudaTypeId *match_type,
    RudaMirRegister *target
);
 bool ruda_checker_private_compile_match_guard(
    RudaFunctionChecker *function,
    const RudaAstNode *guard,
    RudaExpressionValue *result,
    bool *never
);
 bool ruda_checker_private_compile_bool_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaExpressionValue *result
);
 bool ruda_checker_private_function_emit_string_constant(
    RudaFunctionChecker *function,
    RudaMirStringId string_constant,
    size_t start,
    size_t end,
    RudaExpressionValue *result
);
#endif
