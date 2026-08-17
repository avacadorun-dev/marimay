#ifndef RUDA_CHECK_CHECKER_PACKAGE_CONTEXT_H
#define RUDA_CHECK_CHECKER_PACKAGE_CONTEXT_H

#include "checker_declaration_types.h"

typedef struct RudaChecker {
    const RudaAstTree *ast;
    const char *package_name;
    size_t package_name_length;
    const struct RudaCheckImportEnvironment *imports;
    const unsigned char *source;
    size_t source_length;
    RudaCheckOptions options;
    RudaCheckResult *result;
    RudaVector declarations;
    RudaVector constants;
    RudaVector type_aliases;
    RudaVector records;
    RudaVector enums;
    RudaVector interfaces;
    RudaMap top_level_index;
    RudaMap local_nominal_index;
    RudaMap callable_index;
    size_t constant_depth;
    size_t type_alias_depth;
    size_t remaining_constant_steps;
    const RudaVector *active_type_parameters;
    const RudaVector *active_type_arguments;
    RudaVector prelude_enums;
    bool prelude_enums_initialized;
    RudaCheckStatus failure;
} RudaChecker;

#endif
