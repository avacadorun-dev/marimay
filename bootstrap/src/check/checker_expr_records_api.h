#ifndef CHECKER_EXPR_RECORDS_API_H
#define CHECKER_EXPR_RECORDS_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_compile_record_construction(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
);
 bool ruda_checker_private_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
);
 bool ruda_checker_private_resolve_record_member(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *receiver,
    RudaCheckRecordDeclaration **record_declaration,
    RudaCheckRecordFieldDeclaration **field_declaration,
    const RudaCheckedRecordField **imported_field,
    RudaMirRecordId *record,
    size_t *field_index
);
 bool ruda_checker_private_compile_record_member(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
);
#endif
