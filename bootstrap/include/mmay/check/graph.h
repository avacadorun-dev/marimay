#ifndef MMAY_CHECK_GRAPH_H
#define MMAY_CHECK_GRAPH_H

#include <stddef.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/check/package.h"
#include "mmay/check/result.h"

typedef struct MarimayCheckPackageInput {
    const char *import_path;
    size_t import_path_length;
    const MarimayAstTree *const *asts;
    size_t ast_count;
} MarimayCheckPackageInput;

typedef struct MarimayCheckGraphOptions {
    size_t max_packages;
    size_t max_total_imports;
    size_t max_total_bindings;
    size_t max_imports_per_file;
    size_t max_items_per_import;
    size_t max_dependency_depth;
    MarimayCheckOptions package_options;
} MarimayCheckGraphOptions;

MarimayCheckGraphOptions mmay_check_graph_default_options(void);
bool mmay_check_graph_options_valid(
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options
);

/*
 * Checks the root and every reachable supplied local package in one canonical
 * type universe. Import paths are exact decoded package identities; this API
 * performs no filesystem, network, registry, or package-manager operation.
 * Package/file/import input order does not affect symbols or typed MIR. The
 * linked result owns one type table and all reachable functions required by
 * MIR-oracle, bytecode, and native program execution.
 */
MarimayCheckStatus mmay_check_package_graph(
    const MarimayCheckPackageInput *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
);

/* Checks every supplied package, including disconnected module packages. */
MarimayCheckStatus mmay_check_module_graph(
    const MarimayCheckPackageInput *packages,
    size_t package_count,
    MarimayAllocator allocator,
    MarimayCheckGraphOptions options,
    MarimayCheckResult *result
);

#endif
