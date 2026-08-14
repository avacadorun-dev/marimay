#ifndef CHECKER_DECLARATION_FUNCTIONS_API_H
#define CHECKER_DECLARATION_FUNCTIONS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_collect_function_signature(
    RudaChecker *checker,
    const RudaAstNode *node
);
 bool ruda_checker_private_collect_constant_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
);
 bool ruda_checker_private_bind_function_parameters(
    RudaFunctionChecker *function,
    const RudaCheckFunctionDeclaration *declaration
);
 bool ruda_checker_private_check_function_body(
    RudaChecker *checker,
    RudaCheckFunctionDeclaration *declaration
);
 bool ruda_checker_private_infer_constant_expression_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result
);
#endif
