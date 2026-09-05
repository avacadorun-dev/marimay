#ifndef CHECKER_DECLARATION_FUNCTIONS_API_H
#define CHECKER_DECLARATION_FUNCTIONS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_collect_function_signature(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
 bool mmay_checker_private_collect_constant_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
 bool mmay_checker_private_bind_function_parameters(
    MarimayFunctionChecker *function,
    const MarimayCheckFunctionDeclaration *declaration
);
 bool mmay_checker_private_check_function_body(
    MarimayChecker *checker,
    size_t declaration_index
);
 bool mmay_checker_private_infer_constant_expression_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result
);
#endif
