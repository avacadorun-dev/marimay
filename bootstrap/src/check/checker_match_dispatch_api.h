#ifndef CHECKER_MATCH_DISPATCH_API_H
#define CHECKER_MATCH_DISPATCH_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool mmay_checker_private_compile_scalar_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayTypeId scalar_type,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_record_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayCheckRecordDeclaration *record_declaration,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_enum_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
#endif
