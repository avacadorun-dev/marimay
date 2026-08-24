#ifndef MMAY_CHECK_CHECKER_FUNCTION_CONTEXT_H
#define MMAY_CHECK_CHECKER_FUNCTION_CONTEXT_H

#include "checker_package_context.h"

typedef struct MarimayCheckLocal {
    MarimayCheckName name;
    MarimayTypeId type;
    MarimayMirRegister register_id;
    size_t scope_depth;
    bool mutable_binding;
    bool initialized;
} MarimayCheckLocal;

typedef struct MarimayLoopContext {
    MarimayMirBlockId break_target;
    MarimayMirBlockId continue_target;
    size_t scope_depth;
} MarimayLoopContext;

typedef struct MarimayDeferredCall {
    MarimayMirCallId call;
    MarimayMirRegister marker;
    size_t scope_depth;
    MarimayMirSpan span;
} MarimayDeferredCall;

typedef struct MarimayInitializationSnapshot {
    MarimayAllocator allocator;
    unsigned char *states;
    size_t count;
    size_t allocation_size;
} MarimayInitializationSnapshot;

typedef struct MarimayFunctionChecker {
    MarimayChecker *checker;
    MarimayVector locals;
    MarimayVector loops;
    MarimayVector defers;
    MarimayMirFunction mir;
    MarimayMirBlockId block;
    MarimayTypeId result_type;
    size_t diagnostic_start;
    size_t scope_depth;
    bool mir_initialized;
    bool terminated;
    bool constant_evaluation;
    bool unsafe_context;
    bool test_context;
    const MarimayVector *type_parameters;
    const MarimayVector *type_arguments;
} MarimayFunctionChecker;

typedef struct MarimayExpressionValue {
    MarimayTypeId type;
    MarimayMirRegister register_id;
    bool valid;
} MarimayExpressionValue;

#endif
