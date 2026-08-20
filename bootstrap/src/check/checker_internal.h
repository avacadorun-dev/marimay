#ifndef RUDA_CHECK_CHECKER_INTERNAL_H
#define RUDA_CHECK_CHECKER_INTERNAL_H

#include "checker_declaration_types.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/f64.h"
#include "ruda/base/string_literal.h"
#include "ruda/generated/diagnostic_codes.h"

typedef struct RudaChecker RudaChecker;
typedef struct RudaFunctionChecker RudaFunctionChecker;
typedef struct RudaExpressionValue RudaExpressionValue;
typedef struct RudaCheckCallable RudaCheckCallable;

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

/* Shared package-local/imported D003/D043 call resolution. */
bool ruda_check_callable_index_init(RudaChecker *checker);
bool ruda_check_owned_receiver(
    const RudaChecker *checker,
    RudaTypeId type,
    RudaSymbolId *receiver
);
bool ruda_check_callable_insert(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    size_t declaration_index
);
bool ruda_check_callable_remove(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration
);
RudaCheckCallResolution ruda_check_resolve_call(
    const RudaChecker *checker,
    RudaCheckName name,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
);
RudaCheckCallResolution ruda_check_resolve_free_call(
    const RudaChecker *checker,
    RudaCheckName name,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
);
RudaCheckCallResolution ruda_check_resolve_qualified_call(
    const RudaChecker *checker,
    RudaCheckName namespace_name,
    RudaCheckName name,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
);

/* Successful-package definition-fact publication, owned by facts.c. */
bool ruda_check_publish_symbol_seeds(
    RudaChecker *checker,
    const RudaCheckSymbolSeed *seeds,
    size_t seed_count
);
bool ruda_check_record_occurrence(
    RudaChecker *checker,
    RudaCheckName name,
    RudaSymbolId symbol
);
bool ruda_check_publish_completions_for_ast(RudaChecker *checker);
bool ruda_check_finalize_semantic_facts(RudaChecker *checker);

/* Managed-array source lowering, owned by array.c. */
bool ruda_function_ensure_array_layout(
    RudaFunctionChecker *function,
    RudaTypeId type
);
bool ruda_array_layout(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaMirArrayId *result
);
bool ruda_compile_array_literal(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);
bool ruda_compile_array_builtin_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *precompiled_receiver_node,
    const RudaExpressionValue *precompiled_receiver,
    bool *resolved,
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
    const RudaAstNode *precompiled_receiver_node,
    const RudaExpressionValue *precompiled_receiver,
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
bool ruda_check_result_init(
    RudaCheckResult *result,
    RudaAllocator allocator,
    RudaCheckOptions options,
    const RudaSourceSnapshot *snapshot,
    const char *package_name,
    size_t package_name_length
);
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
void ruda_check_dispose_symbol_values(RudaCheckResult *result);
void ruda_check_dispose_completion_values(RudaCheckResult *result);
void ruda_check_dispose_diagnostics(RudaCheckResult *result);
bool ruda_check_clear_semantics(RudaCheckResult *result);
bool ruda_check_function_index_init(RudaCheckResult *result);
bool ruda_check_symbol_index_init(RudaCheckResult *result);
void ruda_check_clear_functions(RudaCheckResult *result);

#endif
