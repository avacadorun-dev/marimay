#ifndef MMAY_CHECK_PACKAGE_H
#define MMAY_CHECK_PACKAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/check/result.h"
#include "mmay/mir/function.h"
#include "mmay/types/type.h"

typedef enum MarimayCheckStatus {
    MMAY_CHECK_OK,
    MMAY_CHECK_INVALID_ARGUMENT,
    MMAY_CHECK_ALLOCATION_FAILED,
    MMAY_CHECK_LIMIT_REACHED,
    MMAY_CHECK_INCOMPLETE_LANGUAGE,
    MMAY_CHECK_INTERNAL_ERROR
} MarimayCheckStatus;

typedef struct MarimayCheckOptions {
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
    MarimayTypeTableOptions type_options;
    MarimayMirFunctionOptions mir_options;
} MarimayCheckOptions;

MarimayCheckOptions mmay_check_default_options(void);
bool mmay_check_options_valid(
    MarimayAllocator allocator,
    MarimayCheckOptions options
);

/*
 * Checks one package after ordering its ASTs by exact source path. Top-level
 * declarations and exact function signatures are collected across every
 * source before any body is resolved. Accepted bodies in the implemented
 * pure-function subset are lowered to typed MIR and verified. Semantic source
 * errors produce diagnostics while status remains MMAY_CHECK_OK. Unsupported
 * accepted language returns MMAY_CHECK_INCOMPLETE_LANGUAGE. Source paths must
 * be unique. ASTs and snapshots remain borrowed; result owns all other data.
 */
MarimayCheckStatus mmay_check_package(
    const MarimayAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    MarimayCheckResult *result
);
/* Convenience wrapper for checking a package containing exactly one AST. */
MarimayCheckStatus mmay_check_ast(
    const MarimayAstTree *ast,
    const char *package_name,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    MarimayCheckResult *result
);

#endif
