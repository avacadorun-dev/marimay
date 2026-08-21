#ifndef RUDA_NATIVE_C_EMITTER_H
#define RUDA_NATIVE_C_EMITTER_H

#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/mir/mir.h"

typedef struct RudaNativeCProgram {
    const RudaMirFunction *const *functions;
    size_t function_count;
    RudaSymbolId entry_symbol;
} RudaNativeCProgram;

typedef struct RudaNativeCEmitOptions {
    size_t max_functions;
    size_t max_generated_bytes;
    size_t max_heap_bytes;
    bool executable_entry;
    bool executable_returns_result;
    RudaSymbolId error_message_symbol;
} RudaNativeCEmitOptions;

typedef enum RudaNativeCEmitStatus {
    RUDA_NATIVE_C_EMIT_OK,
    RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT,
    RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED,
    RUDA_NATIVE_C_EMIT_LIMIT_REACHED,
    RUDA_NATIVE_C_EMIT_INVALID_MIR,
    RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE,
    RUDA_NATIVE_C_EMIT_UNSUPPORTED_INSTRUCTION,
    RUDA_NATIVE_C_EMIT_UNSUPPORTED_TERMINATOR,
    RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL,
    RUDA_NATIVE_C_EMIT_UNSUPPORTED_HOST
} RudaNativeCEmitStatus;

RudaNativeCEmitOptions ruda_native_c_emit_default_options(void);

/* Output is reset on entry and owns the generated C source only on success. */
RudaNativeCEmitStatus ruda_native_c_emit_program(
    const RudaNativeCProgram *program,
    RudaAllocator allocator,
    RudaNativeCEmitOptions options,
    RudaOwnedBytes *output
);

#endif
