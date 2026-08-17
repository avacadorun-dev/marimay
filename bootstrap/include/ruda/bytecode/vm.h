#ifndef RUDA_BYTECODE_VM_H
#define RUDA_BYTECODE_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/map.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/program.h"
#include "ruda/runtime/managed.h"

typedef uint32_t RudaBytecodeReference;

#define RUDA_BYTECODE_INVALID_REFERENCE UINT32_MAX

typedef struct RudaBytecodeValue {
    RudaBytecodeType type;
    union {
        int64_t i64;
        uint64_t f64_bits;
        bool boolean;
        RudaBytecodeReference reference;
        RudaManagedReference managed_reference;
    } as;
} RudaBytecodeValue;

typedef struct RudaBytecodeExecutionOptions {
    size_t max_strings;
    size_t max_total_string_bytes;
    RudaManagedHeapOptions managed_heap;
} RudaBytecodeExecutionOptions;

typedef struct RudaBytecodeExecution {
    RudaAllocator allocator;
    /* Owns RudaOwnedBytes values indexed by RudaBytecodeReference. */
    RudaVector strings;
    /* Exact UTF-8 bytes to RudaBytecodeReference. */
    RudaMap string_index;
    RudaBytecodeExecutionOptions options;
    size_t total_string_bytes;
    RudaManagedHeap managed_heap;
    RudaVector interned_record_descriptors;
    RudaVector interned_record_fields;
    RudaVector interned_enum_descriptors;
    RudaVector interned_enum_variants;
    RudaVector interned_enum_payloads;
    RudaVector owned_interface_tables;
    /* Owned descriptors for generic Array<T>; entries are private VM state. */
    RudaVector array_descriptors;
    bool array_descriptor_allocation_failed;
    bool initialized;
} RudaBytecodeExecution;

/*
 * Managed values are bound to this execution's heap. Active VM frames root
 * every exact managed register. A returned value remains usable until an
 * explicit/subsequent collection reclaims it or the execution is disposed;
 * passing it as the next run's argument roots it before dispatch.  A host that
 * retains values across unrelated collections registers the value's
 * managed_reference through the public RudaManagedRootFrame API.
 */
typedef struct RudaBytecodePanicFrame {
    RudaSymbolId symbol;
    RudaBytecodeSpan call_span;
} RudaBytecodePanicFrame;

typedef struct RudaBytecodePanic {
    const char *code;
    RudaOwnedBytes message;
    RudaSymbolId symbol;
    RudaBytecodeSpan span;
    RudaVector frames;
    bool initialized;
} RudaBytecodePanic;

typedef struct RudaBytecodeTrap {
    RudaBytecodeSpan span;
    bool has_span;
} RudaBytecodeTrap;

typedef struct RudaBytecodeRunOptions {
    /* Per frame, matching the MIR oracle's max_steps_per_function. */
    size_t max_steps;
    /* Root counts as depth one; exhaustion reports STEP_LIMIT. */
    size_t max_call_depth;
    size_t *remaining_shared_steps;
    RudaBytecodeExecution *execution;
    RudaBytecodePanic *panic;
    /* Optional caller-owned output, reset before execution. */
    RudaBytecodeTrap *trap;
} RudaBytecodeRunOptions;

typedef enum RudaBytecodeRunStatus {
    RUDA_BYTECODE_RUN_OK,
    RUDA_BYTECODE_RUN_INVALID_ARGUMENT,
    RUDA_BYTECODE_RUN_ALLOCATION_FAILED,
    RUDA_BYTECODE_RUN_LIMIT_REACHED,
    RUDA_BYTECODE_RUN_INVALID_BYTECODE,
    RUDA_BYTECODE_RUN_STEP_LIMIT,
    RUDA_BYTECODE_RUN_OVERFLOW,
    RUDA_BYTECODE_RUN_DIVISION_BY_ZERO,
    RUDA_BYTECODE_RUN_INVALID_SHIFT,
    RUDA_BYTECODE_RUN_PANIC,
    RUDA_BYTECODE_RUN_INVALID_REFERENCE,
    RUDA_BYTECODE_RUN_UNSUPPORTED_HOST
} RudaBytecodeRunStatus;

RudaBytecodeRunOptions ruda_bytecode_run_default_options(void);
RudaBytecodeExecutionOptions ruda_bytecode_execution_default_options(void);
bool ruda_bytecode_execution_init(
    RudaBytecodeExecution *execution,
    RudaAllocator allocator,
    RudaBytecodeExecutionOptions options
);
void ruda_bytecode_execution_dispose(RudaBytecodeExecution *execution);
bool ruda_bytecode_execution_string_bytes(
    const RudaBytecodeExecution *execution,
    RudaBytecodeValue value,
    const unsigned char **bytes,
    size_t *length
);
bool ruda_bytecode_panic_init(RudaBytecodePanic *panic, RudaAllocator allocator);
void ruda_bytecode_panic_dispose(RudaBytecodePanic *panic);
const RudaBytecodePanicFrame *ruda_bytecode_panic_frame(
    const RudaBytecodePanic *panic,
    size_t index
);
/*
 * Verifies the complete function before allocating its register frame or
 * dispatching any instruction. Each instruction, including a terminator,
 * consumes one step. The result is published only on success. A standalone
 * panic has no program symbol, so its panic symbol and primary frame symbol
 * are the all-zero sentinel; program execution always records exact symbols.
 */
RudaBytecodeRunStatus ruda_bytecode_run(
    const RudaBytecodeFunction *function,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
);
/*
 * Verifies the complete program before allocating execution frames. Direct
 * calls use an explicit heap frame stack, never C recursion. The entry result
 * and call destinations are published only after successful returns.
 */
RudaBytecodeRunStatus ruda_bytecode_run_program(
    const RudaBytecodeProgram *program,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
);
#endif
