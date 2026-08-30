#ifndef MMAY_CHECK_CHECKER_PACKAGE_CONTEXT_H
#define MMAY_CHECK_CHECKER_PACKAGE_CONTEXT_H

#include "checker_declaration_types.h"

typedef struct MarimayChecker {
    const MarimayAstTree *ast;
    const char *package_name;
    size_t package_name_length;
    const struct MarimayCheckImportEnvironment *imports;
    const unsigned char *source;
    size_t source_length;
    MarimayCheckOptions options;
    MarimayCheckResult *result;
    MarimayVector declarations;
    MarimayVector constants;
    MarimayVector type_aliases;
    MarimayVector records;
    MarimayVector enums;
    MarimayVector interfaces;
    MarimayMap top_level_index;
    MarimayMap local_nominal_index;
    MarimayMap callable_index;
    size_t constant_depth;
    size_t type_alias_depth;
    size_t remaining_constant_steps;
    const MarimayVector *active_type_parameters;
    const MarimayVector *active_type_arguments;
    MarimayVector prelude_enums;
    MarimayVector imported_enums;
    bool prelude_enums_initialized;
    bool imported_enums_initialized;
    const MarimayAstNode *incomplete_context;
    const char *incomplete_construct;
    MarimayCheckStatus failure;
} MarimayChecker;

#endif
