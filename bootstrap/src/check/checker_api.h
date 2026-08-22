#ifndef CHECKER_API_H
#define CHECKER_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 int mmay_checker_private_check_compare_ast_paths(const void *left, const void *right);
 bool mmay_checker_private_check_bounded_c_string_length(
    const char *text,
    size_t maximum,
    size_t *length
);
 bool mmay_checker_private_check_ast_valid(const MarimayAstTree *ast);
MarimayCheckStatus mmay_checker_check_package_unit(
    const MarimayAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    size_t package_name_length,
    const struct MarimayCheckImportEnvironment *imports,
    bool initialize_result,
    MarimayAllocator allocator,
    MarimayCheckOptions options,
    MarimayCheckResult *result
);
#endif
