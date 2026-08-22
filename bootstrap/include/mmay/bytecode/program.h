#ifndef MMAY_BYTECODE_PROGRAM_H
#define MMAY_BYTECODE_PROGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/types/type.h"

enum {
    MMAY_BYTECODE_PROGRAM_FORMAT_VERSION = 1
};

typedef struct MarimayBytecodeProgramFunction {
    MarimaySymbolId symbol;
    MarimayBytecodeFunction body;
} MarimayBytecodeProgramFunction;

typedef struct MarimayBytecodeProgramOptions {
    size_t max_functions;
    size_t max_total_registers;
    size_t max_total_blocks;
    size_t max_total_instructions;
    size_t max_total_calls;
    size_t max_total_call_arguments;
    size_t max_total_string_constants;
    size_t max_total_string_bytes;
    size_t max_total_array_constructions;
    size_t max_total_array_arguments;
    size_t max_total_record_layouts;
    size_t max_total_record_fields;
    size_t max_total_record_constructions;
    size_t max_total_record_arguments;
    size_t max_total_enum_layouts;
    size_t max_total_enum_variants;
    size_t max_total_enum_payloads;
    size_t max_total_enum_constructions;
    size_t max_total_enum_arguments;
    size_t max_total_interface_layouts;
    size_t max_total_interface_constructions;
    size_t max_total_interface_methods;
    size_t max_encoded_bytes;
    size_t max_verify_work;
    MarimayBytecodeFunctionOptions function_options;
} MarimayBytecodeProgramOptions;

typedef struct MarimayBytecodeProgram {
    MarimayAllocator allocator;
    size_t source_count;
    MarimayVector functions;
    uint32_t entry_function;
    MarimayBytecodeProgramOptions options;
    bool initialized;
} MarimayBytecodeProgram;

typedef enum MarimayBytecodeProgramVerifyStatus {
    MMAY_BYTECODE_PROGRAM_VERIFY_OK,
    MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT,
    MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED,
    MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED,
    MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION,
    MMAY_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL,
    MMAY_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE,
    MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH
} MarimayBytecodeProgramVerifyStatus;

MarimayBytecodeProgramOptions mmay_bytecode_program_default_options(void);

bool mmay_bytecode_program_init(
    MarimayBytecodeProgram *program,
    MarimayAllocator allocator,
    size_t source_count,
    MarimayBytecodeProgramOptions options
);
void mmay_bytecode_program_dispose(MarimayBytecodeProgram *program);

/*
 * Takes ownership on success and resets function. Symbols must be appended in
 * strictly increasing byte order so function IDs and encoded bytes are stable.
 */
bool mmay_bytecode_program_take_function(
    MarimayBytecodeProgram *program,
    MarimaySymbolId symbol,
    MarimayBytecodeFunction *function,
    uint32_t *result
);
bool mmay_bytecode_program_set_entry(
    MarimayBytecodeProgram *program,
    MarimaySymbolId symbol
);
const MarimayBytecodeProgramFunction *mmay_bytecode_program_function(
    const MarimayBytecodeProgram *program,
    size_t index
);
const MarimayBytecodeProgramFunction *mmay_bytecode_program_function_by_symbol(
    const MarimayBytecodeProgram *program,
    MarimaySymbolId symbol
);

MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_verify(
    const MarimayBytecodeProgram *program
);

/*
 * Internal integrity envelope. It is not the complete public .myb format.
 * Decode checks the SHA-256 payload digest and verifies the whole program
 * before publishing output.
 */
MarimayBytecodeCodecStatus mmay_bytecode_encode_program(
    const MarimayBytecodeProgram *program,
    MarimayAllocator allocator,
    MarimayOwnedBytes *output
);
MarimayBytecodeCodecStatus mmay_bytecode_decode_program(
    const unsigned char *bytes,
    size_t length,
    MarimayAllocator allocator,
    MarimayBytecodeProgramOptions options,
    MarimayBytecodeProgram *output
);

#endif
