#ifndef MMAY_MIR_INTERPRET_H
#define MMAY_MIR_INTERPRET_H

#include "mmay/base/owned_bytes.h"
#include "mmay/mir/execution.h"
#include "mmay/mir/function.h"
#include "mmay/runtime/io.h"

typedef enum MarimayMirInterpretStatus {
    MMAY_MIR_INTERPRET_OK,
    MMAY_MIR_INTERPRET_INVALID_ARGUMENT,
    MMAY_MIR_INTERPRET_ALLOCATION_FAILED,
    MMAY_MIR_INTERPRET_INVALID_MIR,
    MMAY_MIR_INTERPRET_STEP_LIMIT,
    MMAY_MIR_INTERPRET_OVERFLOW,
    MMAY_MIR_INTERPRET_DIVISION_BY_ZERO,
    MMAY_MIR_INTERPRET_INVALID_SHIFT,
    MMAY_MIR_INTERPRET_PANIC,
    MMAY_MIR_INTERPRET_INVALID_REFERENCE,
    MMAY_MIR_INTERPRET_OBJECT_LIMIT,
    MMAY_MIR_INTERPRET_UNSUPPORTED_HOST
} MarimayMirInterpretStatus;

typedef struct MarimayMirSuppressedPanic {
    const char *code;
    MarimayOwnedBytes message;
    MarimaySymbolId symbol;
    MarimayMirSpan span;
} MarimayMirSuppressedPanic;

typedef struct MarimayMirPanic {
    /* Stable explicit-source-panic code from the runtime diagnostic contract. */
    const char *code;
    MarimayOwnedBytes message;
    MarimaySymbolId symbol;
    MarimayMirSpan span;
    MarimayVector suppressed;
} MarimayMirPanic;

typedef MarimayMirInterpretStatus (*MarimayMirCallHandler)(
    void *context,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
);

typedef struct MarimayMirInterpretOptions {
    size_t max_steps;
    size_t *remaining_shared_steps;
    void *call_context;
    MarimayMirCallHandler call;
    MarimayMirExecution *execution;
    const MarimayIoStdout *stdout_sink;
    /* Optional caller-owned zero-initialized output; prior content is disposed. */
    MarimayMirPanic *panic;
} MarimayMirInterpretOptions;

MarimayMirInterpretOptions mmay_mir_interpret_default_options(void);
MarimayMirInterpretStatus mmay_mir_interpret(
    const MarimayMirFunction *function,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayMirInterpretOptions options,
    MarimayMirValue *result
);
void mmay_mir_panic_dispose(MarimayMirPanic *panic);

#endif
