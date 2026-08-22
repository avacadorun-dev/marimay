#ifndef CHECKER_STATEMENT_API_H
#define CHECKER_STATEMENT_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_compile_binding(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_record_assignment(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *left,
    const MarimayAstNode *right,
    const MarimaySyntaxToken *operator_token,
    MarimayMirInstructionKind compound_kind,
    bool compound
);
 bool mmay_checker_private_compile_assignment(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_compile_expression_statement(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
);
 bool mmay_checker_private_function_set_goto(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirBlockId target,
    size_t start,
    size_t end
);
 bool mmay_checker_private_function_set_branch(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false,
    size_t start,
    size_t end
);
 bool mmay_checker_private_function_set_return(
    MarimayFunctionChecker *function,
    MarimayMirRegister value,
    size_t start,
    size_t end
);
 bool mmay_checker_private_function_set_panic(
    MarimayFunctionChecker *function,
    MarimayMirRegister message,
    size_t start,
    size_t end
);
 bool mmay_checker_private_function_set_never_call(
    MarimayFunctionChecker *function,
    MarimayMirCallId call,
    size_t start,
    size_t end
);
#endif
