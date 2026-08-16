#ifndef CHECKER_API_H
#define CHECKER_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 int ruda_checker_private_check_compare_ast_paths(const void *left, const void *right);
 bool ruda_checker_private_check_bounded_c_string_length(
    const char *text,
    size_t maximum,
    size_t *length
);
 bool ruda_checker_private_check_ast_valid(const RudaAstTree *ast);
RudaCheckStatus ruda_checker_check_package_unit(
    const RudaAstTree *const *asts,
    size_t ast_count,
    const char *package_name,
    size_t package_name_length,
    const struct RudaCheckImportEnvironment *imports,
    bool initialize_result,
    RudaAllocator allocator,
    RudaCheckOptions options,
    RudaCheckResult *result
);
#endif
