#ifndef RUDA_CHECK_CHECKER_FUNCTION_CONTEXT_H
#define RUDA_CHECK_CHECKER_FUNCTION_CONTEXT_H

#include "checker_package_context.h"

typedef struct RudaCheckLocal {
    RudaCheckName name;
    RudaTypeId type;
    RudaMirRegister register_id;
    size_t scope_depth;
    bool mutable_binding;
    bool initialized;
} RudaCheckLocal;

typedef struct RudaLoopContext {
    RudaMirBlockId break_target;
    RudaMirBlockId continue_target;
    size_t scope_depth;
} RudaLoopContext;

typedef struct RudaDeferredCall {
    RudaMirCallId call;
    RudaMirRegister marker;
    size_t scope_depth;
    RudaMirSpan span;
} RudaDeferredCall;

typedef struct RudaInitializationSnapshot {
    RudaAllocator allocator;
    unsigned char *states;
    size_t count;
    size_t allocation_size;
} RudaInitializationSnapshot;

typedef struct RudaMatchYieldContext {
    RudaMirBlockId *join;
    RudaTypeId *match_type;
    RudaMirRegister *target;
    bool yielded;
} RudaMatchYieldContext;

typedef struct RudaFunctionChecker {
    RudaChecker *checker;
    RudaVector locals;
    RudaVector loops;
    RudaVector defers;
    RudaMirFunction mir;
    RudaMirBlockId block;
    RudaTypeId result_type;
    size_t diagnostic_start;
    size_t scope_depth;
    RudaMatchYieldContext *match_yield;
    bool mir_initialized;
    bool terminated;
    bool constant_evaluation;
    bool unsafe_context;
    bool test_context;
    const RudaVector *type_parameters;
    const RudaVector *type_arguments;
} RudaFunctionChecker;

typedef struct RudaExpressionValue {
    RudaTypeId type;
    RudaMirRegister register_id;
    bool valid;
} RudaExpressionValue;

#endif
