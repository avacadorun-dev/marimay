#ifndef CHECKER_MATCH_DISPATCH_API_H
#define CHECKER_MATCH_DISPATCH_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool ruda_checker_private_compile_scalar_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaTypeId scalar_type,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_record_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaCheckRecordDeclaration *record_declaration,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_enum_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
#endif
