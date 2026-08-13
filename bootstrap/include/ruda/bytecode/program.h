#ifndef RUDA_BYTECODE_PROGRAM_H
#define RUDA_BYTECODE_PROGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/types/type.h"

enum {
    RUDA_BYTECODE_PROGRAM_FORMAT_VERSION = 1
};

typedef struct RudaBytecodeProgramFunction {
    RudaSymbolId symbol;
    RudaBytecodeFunction body;
} RudaBytecodeProgramFunction;

typedef struct RudaBytecodeProgramOptions {
    size_t max_functions;
    size_t max_total_registers;
    size_t max_total_blocks;
    size_t max_total_instructions;
    size_t max_total_calls;
    size_t max_total_call_arguments;
    size_t max_total_string_constants;
    size_t max_total_string_bytes;
    size_t max_encoded_bytes;
    size_t max_verify_work;
    RudaBytecodeFunctionOptions function_options;
} RudaBytecodeProgramOptions;

typedef struct RudaBytecodeProgram {
    RudaAllocator allocator;
    size_t source_count;
    RudaVector functions;
    uint32_t entry_function;
    RudaBytecodeProgramOptions options;
    bool initialized;
} RudaBytecodeProgram;

typedef enum RudaBytecodeProgramVerifyStatus {
    RUDA_BYTECODE_PROGRAM_VERIFY_OK,
    RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT,
    RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED,
    RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED,
    RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION,
    RUDA_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL,
    RUDA_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE,
    RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH
} RudaBytecodeProgramVerifyStatus;

RudaBytecodeProgramOptions ruda_bytecode_program_default_options(void);

bool ruda_bytecode_program_init(
    RudaBytecodeProgram *program,
    RudaAllocator allocator,
    size_t source_count,
    RudaBytecodeProgramOptions options
);
void ruda_bytecode_program_dispose(RudaBytecodeProgram *program);

/*
 * Takes ownership on success and resets function. Symbols must be appended in
 * strictly increasing byte order so function IDs and encoded bytes are stable.
 */
bool ruda_bytecode_program_take_function(
    RudaBytecodeProgram *program,
    RudaSymbolId symbol,
    RudaBytecodeFunction *function,
    uint32_t *result
);
bool ruda_bytecode_program_set_entry(
    RudaBytecodeProgram *program,
    RudaSymbolId symbol
);
const RudaBytecodeProgramFunction *ruda_bytecode_program_function(
    const RudaBytecodeProgram *program,
    size_t index
);
const RudaBytecodeProgramFunction *ruda_bytecode_program_function_by_symbol(
    const RudaBytecodeProgram *program,
    RudaSymbolId symbol
);

RudaBytecodeProgramVerifyStatus ruda_bytecode_program_verify(
    const RudaBytecodeProgram *program
);

/*
 * Internal integrity envelope. It is not the complete public .rudab format.
 * Decode checks the SHA-256 payload digest and verifies the whole program
 * before publishing output.
 */
RudaBytecodeCodecStatus ruda_bytecode_encode_program(
    const RudaBytecodeProgram *program,
    RudaAllocator allocator,
    RudaOwnedBytes *output
);
RudaBytecodeCodecStatus ruda_bytecode_decode_program(
    const unsigned char *bytes,
    size_t length,
    RudaAllocator allocator,
    RudaBytecodeProgramOptions options,
    RudaBytecodeProgram *output
);

#endif
