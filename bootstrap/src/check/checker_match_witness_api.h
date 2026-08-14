#ifndef CHECKER_MATCH_WITNESS_API_H
#define CHECKER_MATCH_WITNESS_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool ruda_checker_private_match_witness_append(
    RudaChecker *checker,
    RudaVector *buffer,
    const void *bytes,
    size_t length
);
 bool ruda_checker_private_match_witness_append_text(
    RudaChecker *checker,
    RudaVector *buffer,
    const char *text
);
 bool ruda_checker_private_match_witness_append_name(
    RudaChecker *checker,
    RudaVector *buffer,
    RudaCheckName name
);
 bool ruda_checker_private_match_witness_append_size(
    RudaChecker *checker,
    RudaVector *buffer,
    size_t value
);
 bool ruda_checker_private_match_witness_append_u64(
    RudaChecker *checker,
    RudaVector *buffer,
    uint64_t value
);
 bool ruda_checker_private_render_enum_match_witness(
    RudaChecker *checker,
    RudaVector *buffer,
    const RudaCheckEnumVariantDeclaration *variant
);
 bool ruda_checker_private_add_enum_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaCheckEnumDeclaration *declaration,
    const unsigned char *seen_variants,
    const RudaCheckEnumMatchArm *arms,
    size_t arm_count
);
 bool ruda_checker_private_add_bool_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const unsigned char seen[2],
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
);
 bool ruda_checker_private_add_i64_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaMap *seen_literals,
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
);
 const RudaMirStringConstant *ruda_checker_private_check_match_string_constant(
    const RudaFunctionChecker *function,
    RudaMirStringId string_id
);
 bool ruda_checker_private_check_match_string_equal(
    const RudaFunctionChecker *function,
    RudaMirStringId left_id,
    RudaMirStringId right_id
);
 bool ruda_checker_private_add_string_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaMap *seen_literals,
    const RudaCheckScalarMatchArm *arms,
    size_t arm_count
);
 bool ruda_checker_private_add_record_match_witness_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaCheckRecordDeclaration *record,
    bool guarded_witness
);
#endif
