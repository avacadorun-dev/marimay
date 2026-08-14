#ifndef CHECKER_MATCH_PATTERN_API_H
#define CHECKER_MATCH_PATTERN_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool ruda_checker_private_function_add_local(
    RudaFunctionChecker *function,
    RudaCheckName name,
    RudaTypeId type,
    bool mutable_binding,
    bool initialized,
    RudaCheckLocal **result
);
 bool ruda_checker_private_function_emit_copy(
    RudaFunctionChecker *function,
    RudaMirRegister target,
    RudaMirRegister source,
    size_t start,
    size_t end
);
 bool ruda_checker_private_enum_match_name(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaCheckName *name
);
 bool ruda_checker_private_enum_match_add_binding(
    RudaFunctionChecker *function,
    RudaVector *bindings,
    RudaCheckName name,
    RudaTypeId type
);
 bool ruda_checker_private_enum_match_collect_simple_binding(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId type,
    RudaVector *bindings
);
 bool ruda_checker_private_enum_match_validate_positional(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    const RudaCheckEnumVariantDeclaration *variant,
    RudaVector *bindings
);
 bool ruda_checker_private_enum_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckEnumVariantDeclaration *variant,
    RudaVector *bindings
);
 bool ruda_checker_private_record_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckRecordDeclaration *record,
    RudaVector *bindings
);
 bool ruda_checker_private_match_collect_alternatives(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *patterns
);
 bool ruda_checker_private_analyze_enum_match_alternative(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckEnumMatchAlternative *alternative
);
 bool ruda_checker_private_enum_match_payloads_are_refutable(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    bool *result
);
 bool ruda_checker_private_enum_match_bindings_equal(
    const RudaFunctionChecker *function,
    const RudaVector *expected,
    const RudaVector *actual
);
 bool ruda_checker_private_enum_match_alternative_redundant(
    const RudaCheckEnumMatchArm *arm,
    const RudaCheckEnumMatchAlternative *alternative
);
 bool ruda_checker_private_match_arm_body_parts(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    const RudaAstNode **guard,
    const RudaAstNode **body,
    bool *body_is_block
);
#endif
