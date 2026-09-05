#ifndef MMAY_CHECKER_JSON_API_H
#define MMAY_CHECKER_JSON_API_H

#include "checker_function_context.h"

/* Exact package exports, shared by import validation and call selection. */
bool mmay_checker_json_export(const unsigned char *package, size_t package_length,
    const unsigned char *name, size_t name_length);
bool mmay_checker_compile_json(MarimayFunctionChecker *function,
    const MarimayAstNode *node, bool *resolved, MarimayExpressionValue *result);

#endif
