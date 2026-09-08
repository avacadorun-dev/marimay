#ifndef MMAY_CHECKER_ESCAPE_API_H
#define MMAY_CHECKER_ESCAPE_API_H
#include "checker_function_context.h"

/* Checker-only lexical facts; no source semantics or storage changes in MIR. */
bool mmay_escape_local(MarimayFunctionChecker *function, const MarimayCheckLocal *local);
bool mmay_escape_origin(MarimayFunctionChecker *function, MarimayMirRegister value,
    size_t captured_depth, MarimayMirSpan span);
bool mmay_escape_check(MarimayFunctionChecker *function);
void mmay_escape_dispose(MarimayFunctionChecker *function);
#endif
