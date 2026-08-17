#ifndef CHECKER_EXPR_SCALAR_API_H
#define CHECKER_EXPR_SCALAR_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_compile_literal(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_name(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
);
 const RudaAstNode *ruda_checker_private_expression_only_node(
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_unary(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
 bool ruda_checker_private_binary_parts(
    const RudaAstNode *node,
    const RudaAstNode **left,
    const RudaSyntaxToken **operator_token,
    const RudaAstNode **right
);
 bool ruda_checker_private_expression_is_integer_literal(const RudaAstNode *node);
 bool ruda_checker_private_compile_short_circuit(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left_node,
    const RudaSyntaxToken *operator_token,
    const RudaAstNode *right_node,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_binary(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
#endif
