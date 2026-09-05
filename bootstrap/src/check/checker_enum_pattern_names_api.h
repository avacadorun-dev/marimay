#ifndef CHECKER_ENUM_PATTERN_NAMES_API_H
#define CHECKER_ENUM_PATTERN_NAMES_API_H

#include "checker_function_context.h"

/* Resolve a pattern's qualifier against its exact matched enum type. */
bool mmay_checker_enum_pattern_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern_name,
    MarimayTypeId enum_type,
    MarimayCheckName *variant_name
);

#endif
