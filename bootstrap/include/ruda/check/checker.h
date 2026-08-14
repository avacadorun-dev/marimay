#ifndef RUDA_CHECK_CHECKER_H
#define RUDA_CHECK_CHECKER_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/mir/mir.h"
#include "ruda/types/type.h"

typedef enum RudaCheckStatus {
    RUDA_CHECK_OK,
    RUDA_CHECK_INVALID_ARGUMENT,
    RUDA_CHECK_ALLOCATION_FAILED,
    RUDA_CHECK_LIMIT_REACHED,
    RUDA_CHECK_INCOMPLETE_LANGUAGE,
    RUDA_CHECK_INTERNAL_ERROR
} RudaCheckStatus;

typedef struct RudaCheckOptions {
    size_t max_source_files;
    size_t max_functions;
    size_t max_symbols;
    size_t max_package_name_bytes;
    size_t max_constants;
    size_t max_type_aliases;
    size_t max_records;
    size_t max_fields_per_record;
    size_t max_enums;
    size_t max_variants_per_enum;
    size_t max_payload_items_per_variant;
    size_t max_locals_per_function;
    size_t max_diagnostics;
    size_t max_constant_steps;
    size_t max_constant_depth;
    size_t max_type_alias_depth;
    size_t max_float_literal_digits;
    RudaTypeTableOptions type_options;
    RudaMirFunctionOptions mir_options;
} RudaCheckOptions;

typedef struct RudaCheckDiagnostic {
    const char *code;
    const char *message;
    RudaOwnedBytes owned_message;
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
} RudaCheckDiagnostic;

typedef struct RudaCheckedFunction {
    RudaOwnedBytes name;
    RudaTypeId signature_type;
    RudaMirFunction mir;
    bool is_public;
    bool is_test;
    bool is_fuzz;
} RudaCheckedFunction;

typedef struct RudaCheckedConstant {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaMirValue value;
    bool is_public;
} RudaCheckedConstant;

typedef struct RudaCheckedTypeAlias {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId target_type;
    bool is_public;
} RudaCheckedTypeAlias;

typedef struct RudaCheckedRecordField {
    RudaOwnedBytes name;
    RudaTypeId type;
    bool is_public;
    bool is_mutable;
} RudaCheckedRecordField;

typedef struct RudaCheckedRecord {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector fields;
    bool is_public;
} RudaCheckedRecord;

typedef enum RudaCheckedEnumVariantKind {
    RUDA_CHECKED_ENUM_VARIANT_UNIT,
    RUDA_CHECKED_ENUM_VARIANT_POSITIONAL,
    RUDA_CHECKED_ENUM_VARIANT_NAMED
} RudaCheckedEnumVariantKind;

typedef struct RudaCheckedEnumPayloadItem {
    RudaOwnedBytes name;
    RudaTypeId type;
    bool has_name;
} RudaCheckedEnumPayloadItem;

typedef struct RudaCheckedEnumVariant {
    RudaOwnedBytes name;
    RudaCheckedEnumVariantKind kind;
    RudaVector payload;
} RudaCheckedEnumVariant;

typedef struct RudaCheckedEnum {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector variants;
    bool is_public;
} RudaCheckedEnum;

typedef enum RudaCheckedSymbolKind {
    RUDA_CHECKED_SYMBOL_FUNCTION,
    RUDA_CHECKED_SYMBOL_CONSTANT,
    RUDA_CHECKED_SYMBOL_TYPE_ALIAS,
    RUDA_CHECKED_SYMBOL_RECORD,
    RUDA_CHECKED_SYMBOL_ENUM
} RudaCheckedSymbolKind;

/*
 * Successful-package definition fact. type belongs to the containing
 * result's type table. name is result-owned; snapshot is borrowed from the
 * caller-owned AST/source lifetime. Definition spans are half-open bytes.
 */
typedef struct RudaCheckedSymbol {
    RudaOwnedBytes name;
    RudaSymbolId symbol;
    /*
     * Result-local type: function signature, constant value, alias target,
     * or record/enum nominal type according to kind. Never compare this ID
     * across independently produced RudaCheckResult values.
     */
    RudaTypeId type;
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
    RudaCheckedSymbolKind kind;
    bool is_public;
} RudaCheckedSymbol;

typedef struct RudaCheckResult {
    RudaAllocator allocator;
    RudaOwnedBytes package_name;
    /* Borrowed first source snapshot in canonical path order. */
    const RudaSourceSnapshot *snapshot;
    RudaTypeTable types;
    RudaVector diagnostics;
    RudaVector functions;
    RudaVector constants;
    RudaVector type_aliases;
    RudaVector records;
    RudaVector enums;
    RudaVector symbols;
    RudaMap function_index;
    RudaMap symbol_index;
    bool initialized;
} RudaCheckResult;

typedef struct RudaCheckInterpretOptions {
    size_t max_steps_per_function;
    size_t max_total_steps;
    size_t max_call_depth;
    /*
     * Optional caller-owned managed-object lifetime. It is required when a
     * record crosses this API boundary as an argument or result. When omitted,
     * the interpreter owns a temporary execution for internal record values.
     */
    RudaMirExecution *execution;
    /* Optional caller-owned runtime-panic observation for the MIR oracle. */
    RudaMirPanic *panic;
} RudaCheckInterpretOptions;

RudaCheckOptions ruda_check_default_options(void);
bool ruda_check_options_valid(
    RudaAllocator allocator,
    RudaCheckOptions options
);
RudaCheckInterpretOptions ruda_check_interpret_default_options(void);

/*
 * Checks one package after ordering its ASTs by exact source path. Top-level
 * names, supported non-generic type aliases, non-generic nominal record and
 * enum declarations, exact function signatures, and bounded top-level
 * constants are collected across every source before any body is resolved.
 * Accepted
 * bodies in the currently implemented pure-function subset are lowered to
 * typed MIR and verified. Semantic source
 * errors produce diagnostics while the status remains RUDA_CHECK_OK. A
 * syntactically valid baseline construct not implemented by this bootstrap
 * slice returns RUDA_CHECK_INCOMPLETE_LANGUAGE instead of a misleading source
 * diagnostic. Source paths must be unique within asts. The ASTs and source
 * snapshots remain borrowed; the result owns all other data. A checked
 * function's signature_type belongs to result->types.
 */
RudaCheckStatus ruda_check_package(
    const RudaAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
);

/* Convenience wrapper for checking a package containing exactly one AST. */
RudaCheckStatus ruda_check_ast(
    const RudaAstTree *ast,
    const char *package_name,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
);

const RudaCheckDiagnostic *ruda_check_diagnostic(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedFunction *ruda_checked_function(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedConstant *ruda_checked_constant(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedTypeAlias *ruda_checked_type_alias(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedRecord *ruda_checked_record(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedEnum *ruda_checked_enum(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedSymbol *ruda_checked_symbol(
    const RudaCheckResult *result,
    size_t index
);
const RudaCheckedSymbol *ruda_checked_symbol_by_id(
    const RudaCheckResult *result,
    RudaSymbolId symbol
);
/* Exact declaration-name hit only; reference occurrences are not indexed yet. */
const RudaCheckedSymbol *ruda_checked_symbol_defined_at(
    const RudaCheckResult *result,
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset
);
RudaMirInterpretStatus ruda_check_interpret_function(
    const RudaCheckResult *checked,
    size_t function_index,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaCheckInterpretOptions options,
    RudaMirValue *result
);
void ruda_check_result_dispose(RudaCheckResult *result);

#endif
