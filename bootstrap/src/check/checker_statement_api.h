#ifndef CHECKER_STATEMENT_API_H
#define CHECKER_STATEMENT_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_compile_binding(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_record_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left,
    const RudaAstNode *right,
    const RudaSyntaxToken *operator_token,
    RudaMirInstructionKind compound_kind,
    bool compound
);
 bool ruda_checker_private_compile_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_compile_expression_statement(
    RudaFunctionChecker *function,
    const RudaAstNode *node
);
 bool ruda_checker_private_function_set_goto(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirBlockId target,
    size_t start,
    size_t end
);
 bool ruda_checker_private_function_set_branch(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false,
    size_t start,
    size_t end
);
 bool ruda_checker_private_function_set_return(
    RudaFunctionChecker *function,
    RudaMirRegister value,
    size_t start,
    size_t end
);
 bool ruda_checker_private_function_set_panic(
    RudaFunctionChecker *function,
    RudaMirRegister message,
    size_t start,
    size_t end
);
 bool ruda_checker_private_function_set_never_call(
    RudaFunctionChecker *function,
    RudaMirCallId call,
    size_t start,
    size_t end
);
#endif
