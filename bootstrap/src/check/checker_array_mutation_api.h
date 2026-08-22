#ifndef MMAY_CHECKER_ARRAY_MUTATION_API_H
#define MMAY_CHECKER_ARRAY_MUTATION_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

/* Selected D053 []T mutations after receiver-directed resolution. */
bool mmay_checker_private_compile_array_mutation_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *precompiled_receiver_node,
    const MarimayExpressionValue *precompiled_receiver,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_checker_private_array_capture_generation(
    MarimayFunctionChecker *function, MarimayMirArrayId array, MarimayMirRegister receiver,
    size_t start, size_t end, MarimayMirRegister *generation
);
bool mmay_checker_private_array_require_generation(
    MarimayFunctionChecker *function, MarimayMirArrayId array, MarimayTypeId array_type,
    MarimayMirRegister receiver, MarimayMirRegister generation, size_t start, size_t end,
    MarimayMirRegister *checked_receiver
);

#endif
