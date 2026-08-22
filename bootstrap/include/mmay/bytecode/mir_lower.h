#ifndef MMAY_BYTECODE_MIR_LOWER_H
#define MMAY_BYTECODE_MIR_LOWER_H

#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/program.h"
#include "mmay/mir/mir.h"

typedef enum MarimayMirBytecodeLowerStatus {
    MMAY_MIR_BYTECODE_LOWER_OK,
    MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT,
    MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED,
    MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED,
    MMAY_MIR_BYTECODE_LOWER_INVALID_MIR,
    MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE,
    MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_INSTRUCTION,
    MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TERMINATOR
} MarimayMirBytecodeLowerStatus;

typedef struct MarimayMirBytecodeLowerOptions {
    uint64_t source;
    size_t source_count;
    MarimayBytecodeFunctionOptions function_options;
} MarimayMirBytecodeLowerOptions;

typedef struct MarimayMirBytecodeProgramLowerOptions {
    uint64_t source;
    size_t source_count;
    MarimayBytecodeProgramOptions program_options;
} MarimayMirBytecodeProgramLowerOptions;

MarimayMirBytecodeLowerOptions mmay_mir_bytecode_lower_default_options(void);
MarimayMirBytecodeProgramLowerOptions
mmay_mir_bytecode_program_lower_default_options(void);

/*
 * Lowers the verified bool/i64/f64/void/string/never MIR subset supported by
 * the bytecode registry. Unsupported managed semantics fail explicitly. Output
 * must be zero-initialized or a valid initialized bytecode function. Any
 * previous output value is disposed before lowering begins. On failure output
 * is reset and owns no allocations.
 */
MarimayMirBytecodeLowerStatus mmay_mir_lower_bytecode_function(
    const MarimayMirFunction *input,
    MarimayAllocator allocator,
    MarimayMirBytecodeLowerOptions options,
    MarimayBytecodeFunction *output
);

/*
 * Lowers an unordered set of supported verified MIR functions into stable
 * symbol order, resolves the requested entry, and verifies the complete
 * program.
 */
MarimayMirBytecodeLowerStatus mmay_mir_lower_bytecode_program(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimaySymbolId entry,
    MarimayAllocator allocator,
    MarimayMirBytecodeProgramLowerOptions options,
    MarimayBytecodeProgram *output
);

#endif
