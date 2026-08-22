#ifndef MMAY_BYTECODE_VM_H
#define MMAY_BYTECODE_VM_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mmay/base/allocator.h"
#include "mmay/base/map.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/program.h"
#include "mmay/runtime/managed.h"
#include "mmay/runtime/io.h"
#include "mmay/runtime/test_output.h"
typedef uint32_t MarimayBytecodeReference;
#define MMAY_BYTECODE_INVALID_REFERENCE UINT32_MAX
typedef struct MarimayBytecodeValue {
    MarimayBytecodeType type;
    union {
        int64_t i64; uint64_t integer_bits;
        uint64_t f64_bits;
        bool boolean;
        MarimayBytecodeReference reference;
        MarimayManagedReference managed_reference;
    } as;
} MarimayBytecodeValue;
typedef struct MarimayBytecodeExecutionOptions {
    size_t max_strings; size_t max_total_string_bytes;
    MarimayManagedHeapOptions managed_heap;
} MarimayBytecodeExecutionOptions;
typedef struct MarimayBytecodeExecution {
    MarimayAllocator allocator;
    /* Owns MarimayOwnedBytes values indexed by MarimayBytecodeReference. */
    MarimayVector strings;
    /* Exact UTF-8 bytes to MarimayBytecodeReference. */
    MarimayMap string_index;
    MarimayBytecodeExecutionOptions options;
    size_t total_string_bytes; MarimayManagedHeap managed_heap;
    MarimayVector interned_record_descriptors;
    MarimayVector interned_record_fields;
    MarimayVector interned_enum_descriptors;
    MarimayVector interned_enum_variants;
    MarimayVector interned_enum_payloads;
    MarimayVector owned_interface_tables;
    MarimayVector array_descriptors;
    MarimayTestOutput test_output;
    bool array_descriptor_allocation_failed;
    bool initialized;
} MarimayBytecodeExecution;
/*
 * Managed values are bound to this execution's heap. Active VM frames root
 * every exact managed register. A returned value remains usable until an
 * explicit/subsequent collection reclaims it or the execution is disposed;
 * passing it as the next run's argument roots it before dispatch.  A host that
 * retains values across unrelated collections registers the value's
 * managed_reference through the public MarimayManagedRootFrame API.
 */
typedef struct MarimayBytecodePanicFrame {
    MarimaySymbolId symbol;
    MarimayBytecodeSpan call_span;
} MarimayBytecodePanicFrame;
typedef struct MarimayBytecodeSuppressedPanic {
    const char *code;
    MarimayOwnedBytes message;
    MarimaySymbolId symbol;
    MarimayBytecodeSpan span;
    MarimayVector frames;
} MarimayBytecodeSuppressedPanic;
typedef struct MarimayBytecodePanic {
    const char *code;
    MarimayOwnedBytes message;
    MarimaySymbolId symbol;
    MarimayBytecodeSpan span;
    MarimayVector frames;
    MarimayVector suppressed;
    bool initialized;
} MarimayBytecodePanic;
typedef struct MarimayBytecodeTrap {
    MarimayBytecodeSpan span;
    bool has_span;
} MarimayBytecodeTrap;
typedef struct MarimayBytecodeRunOptions {
    /* Per frame, matching the MIR oracle's max_steps_per_function. */
    size_t max_steps;
    /* Root counts as depth one; exhaustion reports STEP_LIMIT. */
    size_t max_call_depth;
    size_t *remaining_shared_steps;
    MarimayBytecodeExecution *execution;
    const MarimayIoStdout *stdout_sink;
    MarimayBytecodePanic *panic;
    /* Optional caller-owned output, reset before execution. */
    MarimayBytecodeTrap *trap;
} MarimayBytecodeRunOptions;
typedef enum MarimayBytecodeRunStatus {
    MMAY_BYTECODE_RUN_OK,
    MMAY_BYTECODE_RUN_INVALID_ARGUMENT,
    MMAY_BYTECODE_RUN_ALLOCATION_FAILED,
    MMAY_BYTECODE_RUN_LIMIT_REACHED,
    MMAY_BYTECODE_RUN_INVALID_BYTECODE,
    MMAY_BYTECODE_RUN_STEP_LIMIT,
    MMAY_BYTECODE_RUN_OVERFLOW,
    MMAY_BYTECODE_RUN_DIVISION_BY_ZERO,
    MMAY_BYTECODE_RUN_INVALID_SHIFT,
    MMAY_BYTECODE_RUN_PANIC,
    MMAY_BYTECODE_RUN_INVALID_REFERENCE,
    MMAY_BYTECODE_RUN_UNSUPPORTED_HOST
} MarimayBytecodeRunStatus;
MarimayBytecodeRunOptions mmay_bytecode_run_default_options(void);
MarimayBytecodeExecutionOptions mmay_bytecode_execution_default_options(void);
bool mmay_bytecode_execution_init(
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator,
    MarimayBytecodeExecutionOptions options
);
void mmay_bytecode_execution_dispose(MarimayBytecodeExecution *execution);
bool mmay_bytecode_execution_string_bytes(
    const MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value,
    const unsigned char **bytes,
    size_t *length
);
bool mmay_bytecode_panic_init(MarimayBytecodePanic *panic, MarimayAllocator allocator);
void mmay_bytecode_panic_dispose(MarimayBytecodePanic *panic);
const MarimayBytecodePanicFrame *mmay_bytecode_panic_frame(
    const MarimayBytecodePanic *panic,
    size_t index
);
const MarimayBytecodeSuppressedPanic *mmay_bytecode_panic_suppressed(
    const MarimayBytecodePanic *panic,
    size_t index
);
/*
 * Verifies the complete function before allocating its register frame or
 * dispatching any instruction. Each instruction, including a terminator,
 * consumes one step. The result is published only on success. A standalone
 * panic has no program symbol, so its panic symbol and primary frame symbol
 * are the all-zero sentinel; program execution always records exact symbols.
 */
MarimayBytecodeRunStatus mmay_bytecode_run(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
);
/*
 * Verifies the complete program before allocating execution frames. Direct
 * calls use an explicit heap frame stack, never C recursion. The entry result
 * and call destinations are published only after successful returns.
 */
MarimayBytecodeRunStatus mmay_bytecode_run_program(
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
);
#endif
