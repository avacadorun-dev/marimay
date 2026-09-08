#ifndef MMAY_CHECKER_CLOSURE_INTERNAL_H
#define MMAY_CHECKER_CLOSURE_INTERNAL_H
#include "checker_closure_api.h"
#include "checker_function_values_internal.h"

typedef struct MarimayClosureCapture {
    MarimayCheckLocal local;
    size_t declaring_depth;
    MarimayMirRecordArgument argument;
} MarimayClosureCapture;

typedef struct MarimayClosureContext {
    MarimayFunctionChecker *parent;
    MarimayVector captures;
    MarimayTypeId environment_type;
    const MarimayAstNode *node;
} MarimayClosureContext;

bool mmay_closure_finish_captures(MarimayFunctionChecker *body,
    MarimayExpressionValue *environment);
#endif
