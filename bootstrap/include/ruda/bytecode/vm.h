#ifndef RUDA_BYTECODE_VM_H
#define RUDA_BYTECODE_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/bytecode/bytecode.h"

typedef struct RudaBytecodeValue {
    RudaBytecodeType type;
    union {
        int64_t i64;
        bool boolean;
    } as;
} RudaBytecodeValue;

typedef struct RudaBytecodeTrap {
    RudaBytecodeSpan span;
    bool has_span;
} RudaBytecodeTrap;

typedef struct RudaBytecodeRunOptions {
    size_t max_steps;
    size_t *remaining_shared_steps;
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
    RUDA_BYTECODE_RUN_INVALID_SHIFT
} RudaBytecodeRunStatus;

RudaBytecodeRunOptions ruda_bytecode_run_default_options(void);

/*
 * Verifies the complete function before allocating its register frame or
 * dispatching any instruction. Each instruction, including a terminator,
 * consumes one step. The result is published only on success.
 */
RudaBytecodeRunStatus ruda_bytecode_run(
    const RudaBytecodeFunction *function,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
);

#endif
