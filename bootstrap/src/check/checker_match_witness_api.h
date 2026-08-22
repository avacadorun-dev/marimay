#ifndef CHECKER_MATCH_WITNESS_API_H
#define CHECKER_MATCH_WITNESS_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool mmay_checker_private_match_witness_append(
    MarimayChecker *checker,
    MarimayVector *buffer,
    const void *bytes,
    size_t length
);
 bool mmay_checker_private_match_witness_append_text(
    MarimayChecker *checker,
    MarimayVector *buffer,
    const char *text
);
 bool mmay_checker_private_match_witness_append_name(
    MarimayChecker *checker,
    MarimayVector *buffer,
    MarimayCheckName name
);
 bool mmay_checker_private_match_witness_append_size(
    MarimayChecker *checker,
    MarimayVector *buffer,
    size_t value
);
 bool mmay_checker_private_match_witness_append_u64(
    MarimayChecker *checker,
    MarimayVector *buffer,
    uint64_t value
);
 bool mmay_checker_private_render_enum_match_witness(
    MarimayChecker *checker,
    MarimayVector *buffer,
    const MarimayCheckEnumVariantDeclaration *variant
);
 bool mmay_checker_private_add_enum_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayCheckEnumDeclaration *declaration,
    const unsigned char *seen_variants,
    const MarimayCheckEnumMatchArm *arms,
    size_t arm_count
);
 bool mmay_checker_private_add_bool_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const unsigned char seen[2],
    const MarimayCheckScalarMatchArm *arms,
    size_t arm_count
);
 bool mmay_checker_private_add_i64_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayMap *seen_literals,
    const MarimayCheckScalarMatchArm *arms,
    size_t arm_count
);
 const MarimayMirStringConstant *mmay_checker_private_check_match_string_constant(
    const MarimayFunctionChecker *function,
    MarimayMirStringId string_id
);
 bool mmay_checker_private_check_match_string_equal(
    const MarimayFunctionChecker *function,
    MarimayMirStringId left_id,
    MarimayMirStringId right_id
);
 bool mmay_checker_private_add_string_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayMap *seen_literals,
    const MarimayCheckScalarMatchArm *arms,
    size_t arm_count
);
 bool mmay_checker_private_add_record_match_witness_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayCheckRecordDeclaration *record,
    bool guarded_witness
);
#endif
