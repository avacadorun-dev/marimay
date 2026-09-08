#ifndef MMAY_CHECKER_CLOSURE_API_H
#define MMAY_CHECKER_CLOSURE_API_H
#include "checker_function_context.h"

const MarimayCheckLocal *mmay_closure_find_captured_local(const MarimayFunctionChecker *function, MarimayCheckName name);

/* Lexical lookup, including lazily materialized enclosing captures. */
MarimayCheckLocal *mmay_closure_find_local(MarimayFunctionChecker *function, MarimayCheckName name);
bool mmay_closure_prepare_local(MarimayFunctionChecker *function, MarimayCheckLocal *local);
bool mmay_closure_read_local(MarimayFunctionChecker *function, const MarimayCheckLocal *local,
    size_t start, size_t end, MarimayExpressionValue *result);
bool mmay_closure_write_local(MarimayFunctionChecker *function, MarimayCheckLocal *local,
    MarimayMirRegister source, size_t start, size_t end);
#endif
