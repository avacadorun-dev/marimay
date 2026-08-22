#ifndef CHECKER_MATCH_PATTERN_API_H
#define CHECKER_MATCH_PATTERN_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool mmay_checker_private_function_add_local(
    MarimayFunctionChecker *function,
    MarimayCheckName name,
    MarimayTypeId type,
    bool mutable_binding,
    bool initialized,
    MarimayCheckLocal **result
);
 bool mmay_checker_private_function_emit_copy(
    MarimayFunctionChecker *function,
    MarimayMirRegister target,
    MarimayMirRegister source,
    size_t start,
    size_t end
);
 bool mmay_checker_private_enum_match_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayCheckName *name
);
 bool mmay_checker_private_enum_match_add_binding(
    MarimayFunctionChecker *function,
    MarimayVector *bindings,
    MarimayCheckName name,
    MarimayTypeId type
);
 bool mmay_checker_private_enum_match_collect_simple_binding(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId type,
    MarimayVector *bindings
);
 bool mmay_checker_private_enum_match_payload_type(
    MarimayFunctionChecker *function,
    MarimayTypeId enum_type,
    const MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId declared_type,
    MarimayTypeId *result
);
 bool mmay_checker_private_enum_match_validate_positional(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    const MarimayCheckEnumDeclaration *enum_declaration,
    const MarimayCheckEnumVariantDeclaration *variant,
    MarimayTypeId enum_type,
    MarimayVector *bindings
);
 bool mmay_checker_private_enum_match_validate_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    const MarimayCheckEnumDeclaration *enum_declaration,
    MarimayCheckEnumVariantDeclaration *variant,
    MarimayTypeId enum_type,
    MarimayVector *bindings
);
 bool mmay_checker_private_record_match_validate_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    MarimayCheckRecordDeclaration *record,
    MarimayVector *bindings
);
 bool mmay_checker_private_match_collect_alternatives(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayVector *patterns
);
 bool mmay_checker_private_analyze_enum_match_alternative(
    MarimayFunctionChecker *function,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId enum_type,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayCheckEnumMatchAlternative *alternative
);
 bool mmay_checker_private_enum_match_payloads_are_refutable(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    bool *result
);
 bool mmay_checker_private_enum_match_bindings_equal(
    const MarimayFunctionChecker *function,
    const MarimayVector *expected,
    const MarimayVector *actual
);
 bool mmay_checker_private_enum_match_alternative_redundant(
    const MarimayCheckEnumMatchArm *arm,
    const MarimayCheckEnumMatchAlternative *alternative
);
 bool mmay_checker_private_match_arm_body_parts(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arm_node,
    const MarimayAstNode **guard,
    const MarimayAstNode **body,
    bool *body_is_block
);
#endif
