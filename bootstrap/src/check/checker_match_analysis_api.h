#ifndef CHECKER_MATCH_ANALYSIS_API_H
#define CHECKER_MATCH_ANALYSIS_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool mmay_checker_private_analyze_bool_match_alternative(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayCheckScalarMatchAlternative *alternative
);
 bool mmay_checker_private_bool_match_alternative_redundant(
    const MarimayCheckScalarMatchArm *arm,
    const MarimayCheckScalarMatchAlternative *alternative
);
 bool mmay_checker_private_analyze_bool_match_arm(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arm_node,
    unsigned char seen[2],
    bool *covered,
    MarimayCheckScalarMatchArm *arm
);
 bool mmay_checker_private_analyze_scalar_match_alternative(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayTypeId scalar_type,
    MarimayCheckScalarMatchAlternative *alternative
);
 bool mmay_checker_private_scalar_match_key(
    const MarimayFunctionChecker *function,
    const MarimayCheckScalarMatchAlternative *alternative,
    MarimayTypeId scalar_type,
    const void **key,
    size_t *key_length
);
 bool mmay_checker_private_scalar_match_alternative_redundant(
    const MarimayFunctionChecker *function,
    const MarimayCheckScalarMatchArm *arm,
    const MarimayCheckScalarMatchAlternative *alternative,
    MarimayTypeId scalar_type
);
 bool mmay_checker_private_analyze_scalar_match_arm(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arm_node,
    MarimayMap *seen_literals,
    bool *covered,
    MarimayTypeId scalar_type,
    MarimayCheckScalarMatchArm *arm
);
 bool mmay_checker_private_resolve_record_match_pattern(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayCheckRecordDeclaration *scrutinee_record,
    MarimayVector *bindings,
    MarimayCheckRecordMatchAlternative *alternative
);
 bool mmay_checker_private_analyze_record_match_arm(
    MarimayFunctionChecker *function,
    MarimayCheckRecordDeclaration *record_declaration,
    const MarimayAstNode *arm_node,
    bool *covered,
    MarimayCheckRecordMatchArm *arm
);
#endif
