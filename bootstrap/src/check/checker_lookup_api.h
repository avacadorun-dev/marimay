#ifndef CHECKER_LOOKUP_API_H
#define CHECKER_LOOKUP_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_check_exact_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result
);
 const RudaCheckLocal *ruda_checker_private_function_find_local(
    const RudaFunctionChecker *function,
    RudaCheckName name
);
 const RudaCheckLocal *ruda_checker_private_function_find_local_in_current_scope(
    const RudaFunctionChecker *function,
    RudaCheckName name
);
 RudaCheckLocal *ruda_checker_private_function_find_local_mutable(
    RudaFunctionChecker *function,
    RudaCheckName name
);
 bool ruda_checker_private_initialization_snapshot_init(
    RudaFunctionChecker *function,
    size_t count,
    RudaInitializationSnapshot *snapshot
);
 bool ruda_checker_private_initialization_snapshot_capture(
    RudaFunctionChecker *function,
    RudaInitializationSnapshot *snapshot
);
 bool ruda_checker_private_initialization_snapshot_restore(
    RudaFunctionChecker *function,
    const RudaInitializationSnapshot *snapshot
);
 void ruda_checker_private_initialization_snapshot_dispose(
    RudaInitializationSnapshot *snapshot
);
 bool ruda_checker_private_function_add_block(
    RudaFunctionChecker *function,
    RudaMirBlockId *result
);
 bool ruda_checker_private_function_emit_f64_constant(
    RudaFunctionChecker *function,
    uint64_t bits,
    size_t start,
    size_t end,
    RudaExpressionValue *result
);
 bool ruda_checker_private_function_record_layout(
    RudaFunctionChecker *function,
    const RudaCheckRecordDeclaration *declaration,
    RudaMirRecordId *result
);
 bool ruda_checker_private_function_enum_layout(
    RudaFunctionChecker *function,
    const RudaCheckEnumDeclaration *declaration,
    RudaMirEnumId *result
);
 bool ruda_checker_private_function_emit_record_access(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId result_type,
    RudaMirRegister receiver,
    RudaMirRegister value,
    RudaMirRecordId record,
    size_t field,
    size_t start,
    size_t end,
    RudaExpressionValue *result
);
 bool ruda_checker_private_integer_magnitude(
    RudaChecker *checker,
    const RudaSyntaxToken *token,
    uint64_t limit,
    uint64_t *result
);
 bool ruda_checker_private_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
);

 bool ruda_checker_private_compile_enum_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);

 bool ruda_checker_private_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);

 bool ruda_checker_private_function_emit_copy(
    RudaFunctionChecker *function,
    RudaMirRegister target,
    RudaMirRegister source,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_goto(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirBlockId target,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_branch(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_panic(
    RudaFunctionChecker *function,
    RudaMirRegister message,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_never_call(
    RudaFunctionChecker *function,
    RudaMirCallId call,
    size_t start,
    size_t end
);

 bool ruda_checker_private_type_is_integer(RudaTypeId type);
 bool ruda_checker_private_type_is_numeric(RudaTypeId type);
#endif
