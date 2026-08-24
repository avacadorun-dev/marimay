#ifndef CHECKER_EXPR_SCALAR_API_H
#define CHECKER_EXPR_SCALAR_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_compile_literal(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
 const MarimayAstNode *mmay_checker_private_expression_only_node(
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_unary(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_binary_parts(
    const MarimayAstNode *node,
    const MarimayAstNode **left,
    const MarimaySyntaxToken **operator_token,
    const MarimayAstNode **right
);
 bool mmay_checker_private_expression_is_integer_literal(const MarimayAstNode *node);
 bool mmay_checker_private_compile_short_circuit(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *left_node,
    const MarimaySyntaxToken *operator_token,
    const MarimayAstNode *right_node,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_binary(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
#endif
