#ifndef CHECKER_CFG_API_H
#define CHECKER_CFG_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_compile_return(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
);

 bool mmay_checker_private_compile_condition(
    MarimayFunctionChecker *function,
    const MarimayAstNode *expression,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_scoped_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
);
 bool mmay_checker_private_merge_initialization_states(
    MarimayFunctionChecker *function,
    const MarimayInitializationSnapshot *when_true,
    bool true_continues,
    const MarimayInitializationSnapshot *when_false,
    bool false_continues
);
 bool mmay_checker_private_compile_if(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_while(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_array_for(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_loop_control(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
);
 bool mmay_checker_private_compile_statement(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
);
#endif
