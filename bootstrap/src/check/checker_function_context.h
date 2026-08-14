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
} RudaLoopContext;

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
    RudaMirFunction mir;
    RudaMirBlockId block;
    RudaTypeId result_type;
    size_t diagnostic_start;
    size_t scope_depth;
    RudaMatchYieldContext *match_yield;
    bool mir_initialized;
    bool terminated;
    bool constant_evaluation;
} RudaFunctionChecker;

typedef struct RudaExpressionValue {
    RudaTypeId type;
    RudaMirRegister register_id;
    bool valid;
} RudaExpressionValue;

#endif
