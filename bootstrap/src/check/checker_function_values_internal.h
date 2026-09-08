#ifndef MMAY_CHECKER_FUNCTION_VALUES_INTERNAL_H
#define MMAY_CHECKER_FUNCTION_VALUES_INTERNAL_H
#include "checker_function_values_api.h"

void mmay_function_value_intern_failed(MarimayFunctionChecker *function);
bool mmay_function_value_init(MarimayFunctionChecker *body, MarimayFunctionChecker *parent,
    MarimaySymbolId symbol, const MarimayTypeId *parameters, size_t count, MarimayTypeId result);
void mmay_function_value_dispose(MarimayFunctionChecker *body);
bool mmay_function_value_record(MarimayFunctionChecker *function, MarimayTypeId type,
    const MarimayMirRecordField *fields, size_t count, MarimayMirRecordId *layout);
bool mmay_function_value_environment(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayMirRecordId layout, const MarimayMirRecordArgument *arguments, size_t count,
    MarimayExpressionValue *result);
#endif
