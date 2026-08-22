#ifndef CHECKER_EXPR_RECORDS_API_H
#define CHECKER_EXPR_RECORDS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_compile_record_construction(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_member_parts(
    const MarimayAstNode *node,
    const MarimayAstNode **receiver,
    const MarimaySyntaxToken **member
);
 bool mmay_checker_private_resolve_record_member(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *receiver,
    MarimayCheckRecordDeclaration **record_declaration,
    MarimayCheckRecordFieldDeclaration **field_declaration,
    const MarimayCheckedRecordField **imported_field,
    MarimayMirRecordId *record,
    size_t *field_index
);
 bool mmay_checker_private_compile_record_member(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *result
);
#endif
