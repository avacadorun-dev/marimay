#ifndef CHECKER_MATCH_ANALYSIS_API_H
#define CHECKER_MATCH_ANALYSIS_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool ruda_checker_private_analyze_bool_match_alternative(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckScalarMatchAlternative *alternative
);
 bool ruda_checker_private_bool_match_alternative_redundant(
    const RudaCheckScalarMatchArm *arm,
    const RudaCheckScalarMatchAlternative *alternative
);
 bool ruda_checker_private_analyze_bool_match_arm(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    unsigned char seen[2],
    bool *covered,
    RudaCheckScalarMatchArm *arm
);
 bool ruda_checker_private_analyze_scalar_match_alternative(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaTypeId scalar_type,
    RudaCheckScalarMatchAlternative *alternative
);
 bool ruda_checker_private_scalar_match_key(
    const RudaFunctionChecker *function,
    const RudaCheckScalarMatchAlternative *alternative,
    RudaTypeId scalar_type,
    const void **key,
    size_t *key_length
);
 bool ruda_checker_private_scalar_match_alternative_redundant(
    const RudaFunctionChecker *function,
    const RudaCheckScalarMatchArm *arm,
    const RudaCheckScalarMatchAlternative *alternative,
    RudaTypeId scalar_type
);
 bool ruda_checker_private_analyze_scalar_match_arm(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    RudaMap *seen_literals,
    bool *covered,
    RudaTypeId scalar_type,
    RudaCheckScalarMatchArm *arm
);
 bool ruda_checker_private_resolve_record_match_pattern(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaCheckRecordDeclaration *scrutinee_record,
    RudaVector *bindings,
    RudaCheckRecordMatchAlternative *alternative
);
 bool ruda_checker_private_analyze_record_match_arm(
    RudaFunctionChecker *function,
    RudaCheckRecordDeclaration *record_declaration,
    const RudaAstNode *arm_node,
    bool *covered,
    RudaCheckRecordMatchArm *arm
);
#endif
