#ifndef RUDA_BYTECODE_MIR_LOWER_H
#define RUDA_BYTECODE_MIR_LOWER_H

#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/program.h"
#include "ruda/mir/mir.h"

typedef enum RudaMirBytecodeLowerStatus {
    RUDA_MIR_BYTECODE_LOWER_OK,
    RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT,
    RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED,
    RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED,
    RUDA_MIR_BYTECODE_LOWER_INVALID_MIR,
    RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE,
    RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_INSTRUCTION,
    RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TERMINATOR
} RudaMirBytecodeLowerStatus;

typedef struct RudaMirBytecodeLowerOptions {
    uint64_t source;
    size_t source_count;
    RudaBytecodeFunctionOptions function_options;
} RudaMirBytecodeLowerOptions;

typedef struct RudaMirBytecodeProgramLowerOptions {
    uint64_t source;
    size_t source_count;
    RudaBytecodeProgramOptions program_options;
} RudaMirBytecodeProgramLowerOptions;

RudaMirBytecodeLowerOptions ruda_mir_bytecode_lower_default_options(void);
RudaMirBytecodeProgramLowerOptions
ruda_mir_bytecode_program_lower_default_options(void);

/*
 * Lowers the verified bool/i64/void/string/never MIR subset supported by the
 * bytecode registry. Unsupported managed semantics fail explicitly. Output
 * must be zero-initialized or a valid initialized bytecode function. Any
 * previous output value is disposed before lowering begins. On failure output
 * is reset and owns no allocations.
 */
RudaMirBytecodeLowerStatus ruda_mir_lower_bytecode_function(
    const RudaMirFunction *input,
    RudaAllocator allocator,
    RudaMirBytecodeLowerOptions options,
    RudaBytecodeFunction *output
);

/*
 * Lowers an unordered set of supported verified MIR functions into stable
 * symbol order, resolves the requested entry, and verifies the complete
 * program.
 */
RudaMirBytecodeLowerStatus ruda_mir_lower_bytecode_program(
    const RudaMirFunction *const *functions,
    size_t function_count,
    RudaSymbolId entry,
    RudaAllocator allocator,
    RudaMirBytecodeProgramLowerOptions options,
    RudaBytecodeProgram *output
);

#endif
