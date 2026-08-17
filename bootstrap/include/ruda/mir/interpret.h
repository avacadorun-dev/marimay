#ifndef RUDA_MIR_INTERPRET_H
#define RUDA_MIR_INTERPRET_H

#include "ruda/base/owned_bytes.h"
#include "ruda/mir/execution.h"
#include "ruda/mir/function.h"

typedef enum RudaMirInterpretStatus {
    RUDA_MIR_INTERPRET_OK,
    RUDA_MIR_INTERPRET_INVALID_ARGUMENT,
    RUDA_MIR_INTERPRET_ALLOCATION_FAILED,
    RUDA_MIR_INTERPRET_INVALID_MIR,
    RUDA_MIR_INTERPRET_STEP_LIMIT,
    RUDA_MIR_INTERPRET_OVERFLOW,
    RUDA_MIR_INTERPRET_DIVISION_BY_ZERO,
    RUDA_MIR_INTERPRET_INVALID_SHIFT,
    RUDA_MIR_INTERPRET_PANIC,
    RUDA_MIR_INTERPRET_INVALID_REFERENCE,
    RUDA_MIR_INTERPRET_OBJECT_LIMIT,
    RUDA_MIR_INTERPRET_UNSUPPORTED_HOST
} RudaMirInterpretStatus;

typedef struct RudaMirSuppressedPanic {
    const char *code;
    RudaOwnedBytes message;
    RudaSymbolId symbol;
    RudaMirSpan span;
} RudaMirSuppressedPanic;

typedef struct RudaMirPanic {
    /* Stable explicit-source-panic code from the runtime diagnostic contract. */
    const char *code;
    RudaOwnedBytes message;
    RudaSymbolId symbol;
    RudaMirSpan span;
    RudaVector suppressed;
} RudaMirPanic;

typedef RudaMirInterpretStatus (*RudaMirCallHandler)(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
);

typedef struct RudaMirInterpretOptions {
    size_t max_steps;
    size_t *remaining_shared_steps;
    void *call_context;
    RudaMirCallHandler call;
    RudaMirExecution *execution;
    /* Optional caller-owned zero-initialized output; prior content is disposed. */
    RudaMirPanic *panic;
} RudaMirInterpretOptions;

RudaMirInterpretOptions ruda_mir_interpret_default_options(void);
RudaMirInterpretStatus ruda_mir_interpret(
    const RudaMirFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaMirInterpretOptions options,
    RudaMirValue *result
);
void ruda_mir_panic_dispose(RudaMirPanic *panic);

#endif
