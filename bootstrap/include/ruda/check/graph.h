#ifndef RUDA_CHECK_GRAPH_H
#define RUDA_CHECK_GRAPH_H

#include <stddef.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/check/package.h"
#include "ruda/check/result.h"

typedef struct RudaCheckPackageInput {
    const char *import_path;
    size_t import_path_length;
    const RudaAstTree *const *asts;
    size_t ast_count;
} RudaCheckPackageInput;

typedef struct RudaCheckGraphOptions {
    size_t max_packages;
    size_t max_total_imports;
    size_t max_total_bindings;
    size_t max_imports_per_file;
    size_t max_items_per_import;
    size_t max_dependency_depth;
    RudaCheckOptions package_options;
} RudaCheckGraphOptions;

RudaCheckGraphOptions ruda_check_graph_default_options(void);
bool ruda_check_graph_options_valid(
    RudaAllocator allocator,
    RudaCheckGraphOptions options
);

/*
 * Checks the root and every reachable supplied local package in one canonical
 * type universe. Import paths are exact decoded package identities; this API
 * performs no filesystem, network, registry, or package-manager operation.
 * Package/file/import input order does not affect symbols or typed MIR. The
 * linked result owns one type table and all reachable functions required by
 * MIR-oracle, bytecode, and native program execution.
 */
RudaCheckStatus ruda_check_package_graph(
    const RudaCheckPackageInput *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
);

#endif
