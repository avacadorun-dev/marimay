#ifndef MMAY_NATIVE_C_EMITTER_H
#define MMAY_NATIVE_C_EMITTER_H

#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/mir/mir.h"

typedef struct MarimayNativeCProgram {
    const MarimayMirFunction *const *functions;
    size_t function_count;
    MarimaySymbolId entry_symbol;
} MarimayNativeCProgram;

typedef struct MarimayNativeCEmitOptions {
    size_t max_functions;
    size_t max_generated_bytes;
    size_t max_heap_bytes;
    bool executable_entry;
    bool executable_takes_arguments;
    bool executable_returns_status;
    bool executable_returns_result;
    MarimaySymbolId error_message_symbol;
} MarimayNativeCEmitOptions;

typedef enum MarimayNativeCEmitStatus {
    MMAY_NATIVE_C_EMIT_OK,
    MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT,
    MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED,
    MMAY_NATIVE_C_EMIT_LIMIT_REACHED,
    MMAY_NATIVE_C_EMIT_INVALID_MIR,
    MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE,
    MMAY_NATIVE_C_EMIT_UNSUPPORTED_INSTRUCTION,
    MMAY_NATIVE_C_EMIT_UNSUPPORTED_TERMINATOR,
    MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL,
    MMAY_NATIVE_C_EMIT_UNSUPPORTED_HOST
} MarimayNativeCEmitStatus;

MarimayNativeCEmitOptions mmay_native_c_emit_default_options(void);

/* Output is reset on entry and owns the generated C source only on success. */
MarimayNativeCEmitStatus mmay_native_c_emit_program(
    const MarimayNativeCProgram *program,
    MarimayAllocator allocator,
    MarimayNativeCEmitOptions options,
    MarimayOwnedBytes *output
);

#endif
