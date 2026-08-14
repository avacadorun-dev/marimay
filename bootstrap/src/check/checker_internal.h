#ifndef RUDA_CHECK_CHECKER_INTERNAL_H
#define RUDA_CHECK_CHECKER_INTERNAL_H

#include "ruda/check/checker.h"

typedef struct RudaCheckName {
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
} RudaCheckName;

typedef struct RudaChecker {
    const RudaAstTree *ast;
    const char *package_name;
    const unsigned char *source;
    size_t source_length;
    RudaCheckOptions options;
    RudaCheckResult *result;
    RudaVector declarations;
    RudaVector constants;
    RudaVector type_aliases;
    RudaVector records;
    RudaVector enums;
    RudaMap top_level_index;
    size_t constant_depth;
    size_t type_alias_depth;
    size_t remaining_constant_steps;
    RudaCheckStatus failure;
} RudaChecker;

typedef struct RudaMatchYieldContext {
    RudaMirBlockId *join;
    RudaTypeId *match_type;
    RudaMirRegister *target;
    bool yielded;
} RudaMatchYieldContext;

typedef struct RudaFunctionChecker {
    RudaChecker *checker;
    RudaVector locals;
    RudaVector loops;
    RudaMirFunction mir;
    RudaMirBlockId block;
    RudaTypeId result_type;
    size_t diagnostic_start;
    size_t scope_depth;
    RudaMatchYieldContext *match_yield;
    bool mir_initialized;
    bool terminated;
    bool constant_evaluation;
} RudaFunctionChecker;

typedef struct RudaExpressionValue {
    RudaTypeId type;
    RudaMirRegister register_id;
    bool valid;
} RudaExpressionValue;

/* Shared expression-lowering boundary for owner-private sibling modules. */
void ruda_checker_fail(RudaChecker *checker, RudaCheckStatus status);
bool ruda_checker_has_failed(const RudaChecker *checker);
bool ruda_checker_add_diagnostic(
    RudaChecker *checker,
    const char *code,
    const char *message,
    size_t start,
    size_t end
);
bool ruda_expression_is_never(
    const RudaFunctionChecker *function,
    const RudaExpressionValue *value
);
bool ruda_function_add_register(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaMirRegister *result
);
bool ruda_function_append_instruction(
    RudaFunctionChecker *function,
    const RudaMirInstruction *instruction
);
bool ruda_function_emit_instruction(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId type,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start,
    size_t end,
    RudaExpressionValue *result
);
bool ruda_compile_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
bool ruda_compound_assignment_kind(
    RudaTypeId type,
    RudaTokenKind operator_kind,
    RudaMirInstructionKind *kind
);

/* Managed-array source lowering, owned by array.c. */
bool ruda_function_ensure_array_layout(
    RudaFunctionChecker *function,
    RudaTypeId type
);
bool ruda_compile_array_literal(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
bool ruda_compile_array_index(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
);
bool ruda_compile_array_length_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool builtin_available,
    bool *resolved,
    RudaExpressionValue *result
);
bool ruda_compile_array_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left,
    const RudaAstNode *right,
    const RudaSyntaxToken *operator_token,
    bool compound
);

/* Result-owner lifecycle used by package publication and rollback. */
void ruda_checked_record_value_dispose(RudaCheckedRecord *record);
void ruda_checked_enum_variant_value_dispose(
    RudaCheckedEnumVariant *variant
);
void ruda_checked_enum_value_dispose(RudaCheckedEnum *enum_value);
void ruda_check_dispose_function_values(RudaCheckResult *result);
void ruda_check_dispose_constant_values(RudaCheckResult *result);
void ruda_check_dispose_type_alias_values(RudaCheckResult *result);
void ruda_check_dispose_record_values(RudaCheckResult *result);
void ruda_check_dispose_enum_values(RudaCheckResult *result);
void ruda_check_dispose_diagnostics(RudaCheckResult *result);
bool ruda_check_function_index_init(RudaCheckResult *result);
void ruda_check_clear_functions(RudaCheckResult *result);

#endif
