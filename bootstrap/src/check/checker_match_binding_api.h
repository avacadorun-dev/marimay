#ifndef CHECKER_MATCH_BINDING_API_H
#define CHECKER_MATCH_BINDING_API_H

#include "checker_internal.h"
#include "checker_match_types.h"

 bool mmay_checker_private_analyze_enum_match_arm(
    MarimayFunctionChecker *function,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId enum_type,
    const MarimayAstNode *arm_node,
    unsigned char *seen_variants,
    bool *covered,
    MarimayCheckEnumMatchArm *arm
);
 bool mmay_checker_private_declare_match_bindings(
    MarimayFunctionChecker *function,
    const MarimayVector *bindings
);
 bool mmay_checker_private_assign_match_local(
    MarimayFunctionChecker *function,
    MarimayCheckName name,
    MarimayTypeId type,
    MarimayMirRegister source,
    size_t start,
    size_t end
);
 bool mmay_checker_private_bind_enum_match_payload(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister guarded,
    size_t payload_index,
    const MarimayAstNode *binding,
    MarimayCheckName shorthand
);
 bool mmay_checker_private_compile_enum_match_bindings(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister receiver
);
 bool mmay_checker_private_compile_enum_match_bindings_cps(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister receiver,
    MarimayMirBlockId success,
    MarimayMirBlockId failure
);
 bool mmay_checker_private_compile_record_match_bindings(
    MarimayFunctionChecker *function,
    const MarimayCheckRecordMatchAlternative *alternative,
    MarimayCheckRecordDeclaration *record_declaration,
    MarimayMirRecordId record,
    MarimayMirRegister receiver
);
#endif
