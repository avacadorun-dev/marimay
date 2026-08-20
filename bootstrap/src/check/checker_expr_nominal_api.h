#ifndef CHECKER_EXPR_NOMINAL_API_H
#define CHECKER_EXPR_NOMINAL_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_resolve_enum_member(
    RudaFunctionChecker *function,
    const RudaAstNode *member_node,
    RudaTypeId expected,
    RudaCheckEnumDeclaration **enum_declaration,
    RudaCheckEnumVariantDeclaration **variant_declaration,
    size_t *variant_index,
    bool *resolved
);
 bool ruda_checker_private_function_emit_enum_construction(
    RudaFunctionChecker *function,
    const RudaCheckEnumDeclaration *enum_declaration,
    size_t variant_index,
    const RudaMirEnumArgument *arguments,
    size_t argument_count,
    size_t start,
    size_t end,
    RudaExpressionValue *result
);
 bool ruda_checker_private_function_emit_enum_access(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId result_type,
    RudaMirRegister receiver,
    RudaMirEnumId enum_id,
    size_t variant,
    size_t payload,
    size_t start,
    size_t end,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_enum_unit(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_enum_positional(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);
 bool ruda_checker_private_compile_enum_named(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
);
#endif
