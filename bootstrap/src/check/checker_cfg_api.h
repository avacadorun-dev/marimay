#ifndef CHECKER_CFG_API_H
#define CHECKER_CFG_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_compile_return(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);

 bool ruda_checker_private_compile_condition(
    RudaFunctionChecker *function,
    const RudaAstNode *expression,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_scoped_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);
 bool ruda_checker_private_merge_initialization_states(
    RudaFunctionChecker *function,
    const RudaInitializationSnapshot *when_true,
    bool true_continues,
    const RudaInitializationSnapshot *when_false,
    bool false_continues
);
 bool ruda_checker_private_compile_if(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_while(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_c_for(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_array_for(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_loop_control(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
);
 bool ruda_checker_private_compile_statement(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
);
#endif
