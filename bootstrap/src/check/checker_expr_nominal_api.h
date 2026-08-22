#ifndef CHECKER_EXPR_NOMINAL_API_H
#define CHECKER_EXPR_NOMINAL_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_resolve_enum_member(
    MarimayFunctionChecker *function,
    const MarimayAstNode *member_node,
    MarimayTypeId expected,
    MarimayCheckEnumDeclaration **enum_declaration,
    MarimayCheckEnumVariantDeclaration **variant_declaration,
    size_t *variant_index,
    bool *resolved
);
 bool mmay_checker_private_function_emit_enum_construction(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *enum_declaration,
    size_t variant_index,
    const MarimayMirEnumArgument *arguments,
    size_t argument_count,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_function_emit_enum_access(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId result_type,
    MarimayMirRegister receiver,
    MarimayMirEnumId enum_id,
    size_t variant,
    size_t payload,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_enum_unit(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_enum_positional(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_compile_enum_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);
#endif
