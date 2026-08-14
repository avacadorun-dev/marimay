#ifndef CHECKER_MATCH_BINDING_API_H
#define CHECKER_MATCH_BINDING_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool ruda_checker_private_analyze_enum_match_arm(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *arm_node,
    unsigned char *seen_variants,
    bool *covered,
    RudaCheckEnumMatchArm *arm
);
 bool ruda_checker_private_declare_match_bindings(
    RudaFunctionChecker *function,
    const RudaVector *bindings
);
 bool ruda_checker_private_assign_match_local(
    RudaFunctionChecker *function,
    RudaCheckName name,
    RudaTypeId type,
    RudaMirRegister source,
    size_t start,
    size_t end
);
 bool ruda_checker_private_bind_enum_match_payload(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister guarded,
    size_t payload_index,
    const RudaAstNode *binding,
    RudaCheckName shorthand
);
 bool ruda_checker_private_compile_enum_match_bindings(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister receiver
);
 bool ruda_checker_private_compile_record_match_bindings(
    RudaFunctionChecker *function,
    const RudaCheckRecordMatchAlternative *alternative,
    RudaCheckRecordDeclaration *record_declaration,
    RudaMirRecordId record,
    RudaMirRegister receiver
);
#endif
