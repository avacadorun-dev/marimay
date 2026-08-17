#ifndef RUDA_CHECK_PACKAGE_H
#define RUDA_CHECK_PACKAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/check/result.h"
#include "ruda/mir/function.h"
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
    size_t max_occurrences;
    size_t max_completions;
    size_t max_package_name_bytes;
    size_t max_constants;
    size_t max_type_aliases;
    size_t max_records;
    size_t max_fields_per_record;
    size_t max_enums;
    size_t max_variants_per_enum;
    size_t max_payload_items_per_variant;
    size_t max_interfaces;
    size_t max_methods_per_interface;
    size_t max_parameters_per_interface_method;
    size_t max_locals_per_function;
    size_t max_diagnostics;
    size_t max_constant_steps;
    size_t max_constant_depth;
    size_t max_type_alias_depth;
    size_t max_float_literal_digits;
    RudaTypeTableOptions type_options;
    RudaMirFunctionOptions mir_options;
} RudaCheckOptions;

RudaCheckOptions ruda_check_default_options(void);
bool ruda_check_options_valid(
    RudaAllocator allocator,
    RudaCheckOptions options
);

/*
 * Checks one package after ordering its ASTs by exact source path. Top-level
 * declarations and exact function signatures are collected across every
 * source before any body is resolved. Accepted bodies in the implemented
 * pure-function subset are lowered to typed MIR and verified. Semantic source
 * errors produce diagnostics while status remains RUDA_CHECK_OK. Unsupported
 * accepted language returns RUDA_CHECK_INCOMPLETE_LANGUAGE. Source paths must
 * be unique. ASTs and snapshots remain borrowed; result owns all other data.
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

#endif
