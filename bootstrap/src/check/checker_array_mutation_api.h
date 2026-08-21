#ifndef RUDA_CHECKER_ARRAY_MUTATION_API_H
#define RUDA_CHECKER_ARRAY_MUTATION_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

/* Selected D053 Array<T> mutations after receiver-directed resolution. */
bool ruda_checker_private_compile_array_mutation_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *precompiled_receiver_node,
    const RudaExpressionValue *precompiled_receiver,
    bool *resolved,
    RudaExpressionValue *result
);
bool ruda_checker_private_array_capture_generation(
    RudaFunctionChecker *function, RudaMirArrayId array, RudaMirRegister receiver,
    size_t start, size_t end, RudaMirRegister *generation
);
bool ruda_checker_private_array_require_generation(
    RudaFunctionChecker *function, RudaMirArrayId array, RudaTypeId array_type,
    RudaMirRegister receiver, RudaMirRegister generation, size_t start, size_t end,
    RudaMirRegister *checked_receiver
);

#endif
