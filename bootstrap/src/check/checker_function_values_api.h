#ifndef MMAY_CHECKER_FUNCTION_VALUES_API_H
#define MMAY_CHECKER_FUNCTION_VALUES_API_H

#include "checker_function_context.h"

MarimayTypeId mmay_function_value_abi_result(MarimayTypeId type);
bool mmay_function_value_layout(MarimayFunctionChecker *function, MarimayTypeId type);
bool mmay_function_value_named(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, bool *resolved, MarimayExpressionValue *result);
bool mmay_function_value_closure(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, MarimayExpressionValue *result);
bool mmay_function_value_call(MarimayFunctionChecker *function, const MarimayAstNode *node,
    bool *resolved, MarimayExpressionValue *result);
bool mmay_function_value_invoke(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayExpressionValue callee, const MarimayAstNode *arguments, MarimayExpressionValue *result);
bool mmay_function_value_symbol(MarimayFunctionChecker *function, size_t location,
    unsigned tag, MarimaySymbolId *symbol, MarimayTypeId *type);
bool mmay_function_value_publish(MarimayFunctionChecker *function, MarimayTypeId signature);
bool mmay_function_value_wrap(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId type, MarimayExpressionValue environment, MarimaySymbolId body,
    MarimayExpressionValue *result);

#endif
