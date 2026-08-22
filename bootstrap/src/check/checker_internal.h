#ifndef MMAY_CHECK_CHECKER_INTERNAL_H
#define MMAY_CHECK_CHECKER_INTERNAL_H

#include "checker_declaration_types.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/f64.h"
#include "mmay/base/string_literal.h"
#include "mmay/generated/diagnostic_codes.h"

typedef struct MarimayChecker MarimayChecker;
typedef struct MarimayFunctionChecker MarimayFunctionChecker;
typedef struct MarimayExpressionValue MarimayExpressionValue;
typedef struct MarimayCheckCallable MarimayCheckCallable;

/* Shared expression-lowering boundary for owner-private sibling modules. */
void mmay_checker_fail(MarimayChecker *checker, MarimayCheckStatus status);
bool mmay_checker_has_failed(const MarimayChecker *checker);
bool mmay_checker_add_diagnostic(
    MarimayChecker *checker,
    const char *code,
    const char *message,
    size_t start,
    size_t end
);
bool mmay_expression_is_never(
    const MarimayFunctionChecker *function,
    const MarimayExpressionValue *value
);
bool mmay_function_add_register(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister *result
);
bool mmay_function_append_instruction(
    MarimayFunctionChecker *function,
    const MarimayMirInstruction *instruction
);
bool mmay_function_emit_instruction(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId type,
    MarimayMirRegister first,
    MarimayMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
);
bool mmay_compile_expression(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
bool mmay_compound_assignment_kind(
    MarimayTypeId type,
    MarimayTokenKind operator_kind,
    MarimayMirInstructionKind *kind
);

/* Shared package-local/imported D003/D043 call resolution. */
bool mmay_check_callable_index_init(MarimayChecker *checker);
bool mmay_check_owned_receiver(
    const MarimayChecker *checker,
    MarimayTypeId type,
    MarimaySymbolId *receiver
);
bool mmay_check_callable_insert(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    size_t declaration_index
);
bool mmay_check_callable_remove(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration
);
MarimayCheckCallResolution mmay_check_resolve_call(
    const MarimayChecker *checker,
    MarimayCheckName name,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
);
MarimayCheckCallResolution mmay_check_resolve_free_call(
    const MarimayChecker *checker,
    MarimayCheckName name,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
);
MarimayCheckCallResolution mmay_check_resolve_qualified_call(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    MarimayCheckName name,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
);

/* Successful-package definition-fact publication, owned by facts.c. */
bool mmay_check_publish_symbol_seeds(
    MarimayChecker *checker,
    const MarimayCheckSymbolSeed *seeds,
    size_t seed_count
);
bool mmay_check_record_occurrence(
    MarimayChecker *checker,
    MarimayCheckName name,
    MarimaySymbolId symbol
);
bool mmay_check_publish_completions_for_ast(MarimayChecker *checker);
bool mmay_check_finalize_semantic_facts(MarimayChecker *checker);

/* Managed-array source lowering, owned by array.c. */
bool mmay_function_ensure_array_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type
);
bool mmay_array_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirArrayId *result
);
bool mmay_compile_array_literal(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);
bool mmay_compile_array_builtin_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *precompiled_receiver_node,
    const MarimayExpressionValue *precompiled_receiver,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_compile_array_index(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *result
);
bool mmay_compile_array_length_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool builtin_available,
    const MarimayAstNode *precompiled_receiver_node,
    const MarimayExpressionValue *precompiled_receiver,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_compile_array_assignment(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *left,
    const MarimayAstNode *right,
    const MarimaySyntaxToken *operator_token,
    bool compound
);

/* Result-owner lifecycle used by package publication and rollback. */
bool mmay_check_result_init(
    MarimayCheckResult *result,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    const MarimaySourceSnapshot *snapshot,
    const char *package_name,
    size_t package_name_length
);
void mmay_checked_record_value_dispose(MarimayCheckedRecord *record);
void mmay_checked_enum_variant_value_dispose(
    MarimayCheckedEnumVariant *variant
);
void mmay_checked_enum_value_dispose(MarimayCheckedEnum *enum_value);
void mmay_check_dispose_function_values(MarimayCheckResult *result);
void mmay_check_dispose_constant_values(MarimayCheckResult *result);
void mmay_check_dispose_type_alias_values(MarimayCheckResult *result);
void mmay_check_dispose_record_values(MarimayCheckResult *result);
void mmay_check_dispose_enum_values(MarimayCheckResult *result);
void mmay_check_dispose_symbol_values(MarimayCheckResult *result);
void mmay_check_dispose_completion_values(MarimayCheckResult *result);
void mmay_check_dispose_diagnostics(MarimayCheckResult *result);
bool mmay_check_clear_semantics(MarimayCheckResult *result);
bool mmay_check_function_index_init(MarimayCheckResult *result);
bool mmay_check_symbol_index_init(MarimayCheckResult *result);
void mmay_check_clear_functions(MarimayCheckResult *result);

#endif
