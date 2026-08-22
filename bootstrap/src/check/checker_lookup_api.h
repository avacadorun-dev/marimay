#ifndef CHECKER_LOOKUP_API_H
#define CHECKER_LOOKUP_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_check_exact_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result
);
 const MarimayCheckLocal *mmay_checker_private_function_find_local(
    const MarimayFunctionChecker *function,
    MarimayCheckName name
);
 const MarimayCheckLocal *mmay_checker_private_function_find_local_in_current_scope(
    const MarimayFunctionChecker *function,
    MarimayCheckName name
);
 MarimayCheckLocal *mmay_checker_private_function_find_local_mutable(
    MarimayFunctionChecker *function,
    MarimayCheckName name
);
 bool mmay_checker_private_initialization_snapshot_init(
    MarimayFunctionChecker *function,
    size_t count,
    MarimayInitializationSnapshot *snapshot
);
 bool mmay_checker_private_initialization_snapshot_capture(
    MarimayFunctionChecker *function,
    MarimayInitializationSnapshot *snapshot
);
 bool mmay_checker_private_initialization_snapshot_restore(
    MarimayFunctionChecker *function,
    const MarimayInitializationSnapshot *snapshot
);
 void mmay_checker_private_initialization_snapshot_dispose(
    MarimayInitializationSnapshot *snapshot
);
 bool mmay_checker_private_function_add_block(
    MarimayFunctionChecker *function,
    MarimayMirBlockId *result
);
 bool mmay_checker_private_function_emit_f64_constant(
    MarimayFunctionChecker *function,
    uint64_t bits,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_function_record_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckRecordDeclaration *declaration,
    MarimayMirRecordId *result
);
 bool mmay_checker_private_function_enum_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *declaration,
    MarimayMirEnumId *result
);
 bool mmay_checker_private_function_emit_record_access(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId result_type,
    MarimayMirRegister receiver,
    MarimayMirRegister value,
    MarimayMirRecordId record,
    size_t field,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_integer_magnitude(
    MarimayChecker *checker,
    const MarimaySyntaxToken *token,
    uint64_t limit,
    uint64_t *result
);
 bool mmay_checker_private_member_parts(
    const MarimayAstNode *node,
    const MarimayAstNode **receiver,
    const MarimaySyntaxToken **member
);

 bool mmay_checker_private_compile_enum_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);

 bool mmay_checker_private_compile_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
);

 bool mmay_checker_private_function_emit_copy(
    MarimayFunctionChecker *function,
    MarimayMirRegister target,
    MarimayMirRegister source,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_goto(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirBlockId target,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_branch(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_panic(
    MarimayFunctionChecker *function,
    MarimayMirRegister message,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_never_call(
    MarimayFunctionChecker *function,
    MarimayMirCallId call,
    size_t start,
    size_t end
);

 bool mmay_checker_private_type_is_integer(MarimayTypeId type);
 bool mmay_checker_private_type_is_numeric(MarimayTypeId type);
#endif
