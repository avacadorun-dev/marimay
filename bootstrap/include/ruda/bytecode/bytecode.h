#ifndef RUDA_BYTECODE_BYTECODE_H
#define RUDA_BYTECODE_BYTECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/base/writer.h"
#include "ruda/generated/opcodes.h"
#include "ruda/types/type.h"

enum {
    RUDA_BYTECODE_FORMAT_VERSION = 1
};

typedef enum RudaBytecodeOperandKind {
    RUDA_BYTECODE_OPERAND_NONE,
    RUDA_BYTECODE_OPERAND_REGISTER,
    RUDA_BYTECODE_OPERAND_BLOCK,
    RUDA_BYTECODE_OPERAND_CALL,
    RUDA_BYTECODE_OPERAND_I64,
    RUDA_BYTECODE_OPERAND_BOOL
} RudaBytecodeOperandKind;

enum {
    RUDA_OPCODE_FLAG_INVALID = 1U << 0,
    RUDA_OPCODE_FLAG_TERMINATOR = 1U << 1,
    RUDA_OPCODE_FLAG_BRANCH = 1U << 2,
    RUDA_OPCODE_FLAG_MAY_PANIC = 1U << 3
};

typedef struct RudaOpcodeMetadata {
    RudaOpcode opcode;
    uint8_t wire_id;
    const char *mnemonic;
    uint8_t operand_count;
    RudaBytecodeOperandKind operands[3];
    uint32_t flags;
} RudaOpcodeMetadata;

typedef enum RudaBytecodeType {
    RUDA_BYTECODE_TYPE_INVALID,
    RUDA_BYTECODE_TYPE_BOOL,
    RUDA_BYTECODE_TYPE_I64,
    RUDA_BYTECODE_TYPE_VOID
} RudaBytecodeType;

typedef struct RudaBytecodeSpan {
    uint64_t source;
    uint64_t start;
    uint64_t end;
} RudaBytecodeSpan;

typedef struct RudaBytecodeOperand {
    uint64_t unsigned_value;
    int64_t signed_value;
} RudaBytecodeOperand;

typedef struct RudaBytecodeInstruction {
    RudaOpcode opcode;
    RudaBytecodeOperand operands[3];
    RudaBytecodeSpan span;
} RudaBytecodeInstruction;

typedef struct RudaBytecodeBlock {
    RudaAllocator allocator;
    RudaVector instructions;
    bool initialized;
} RudaBytecodeBlock;

typedef uint32_t RudaBytecodeCallId;

#define RUDA_BYTECODE_NO_CALL UINT32_MAX

typedef struct RudaBytecodeCallArgument {
    uint32_t register_id;
    RudaBytecodeType type;
} RudaBytecodeCallArgument;

typedef struct RudaBytecodeCall {
    RudaSymbolId callee;
    RudaBytecodeType result_type;
    size_t argument_start;
    size_t argument_count;
} RudaBytecodeCall;

typedef struct RudaBytecodeFunctionOptions {
    size_t max_registers;
    size_t max_blocks;
    size_t max_instructions;
    size_t max_calls;
    size_t max_call_arguments;
    size_t max_source_files;
    size_t max_encoded_bytes;
    size_t max_verify_work;
} RudaBytecodeFunctionOptions;

typedef struct RudaBytecodeFunction {
    RudaAllocator allocator;
    RudaBytecodeType result_type;
    size_t source_count;
    size_t parameter_count;
    RudaVector register_types;
    RudaVector blocks;
    RudaVector calls;
    RudaVector call_arguments;
    size_t instruction_count;
    RudaBytecodeFunctionOptions options;
    bool initialized;
} RudaBytecodeFunction;

typedef enum RudaBytecodeVerifyStatus {
    RUDA_BYTECODE_VERIFY_OK,
    RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT,
    RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED,
    RUDA_BYTECODE_VERIFY_LIMIT_REACHED,
    RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW,
    RUDA_BYTECODE_VERIFY_INVALID_OPCODE,
    RUDA_BYTECODE_VERIFY_INVALID_REGISTER,
    RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER,
    RUDA_BYTECODE_VERIFY_TYPE_MISMATCH
} RudaBytecodeVerifyStatus;

typedef enum RudaBytecodeCodecStatus {
    RUDA_BYTECODE_CODEC_OK,
    RUDA_BYTECODE_CODEC_INVALID_ARGUMENT,
    RUDA_BYTECODE_CODEC_ALLOCATION_FAILED,
    RUDA_BYTECODE_CODEC_LIMIT_REACHED,
    RUDA_BYTECODE_CODEC_TRUNCATED,
    RUDA_BYTECODE_CODEC_INVALID_ENCODING,
    RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION,
    RUDA_BYTECODE_CODEC_VERIFY_FAILED,
    RUDA_BYTECODE_CODEC_TRAILING_DATA
} RudaBytecodeCodecStatus;

typedef enum RudaBytecodeDisassembleStatus {
    RUDA_BYTECODE_DISASSEMBLE_OK,
    RUDA_BYTECODE_DISASSEMBLE_INVALID_ARGUMENT,
    RUDA_BYTECODE_DISASSEMBLE_INVALID_BYTECODE,
    RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED
} RudaBytecodeDisassembleStatus;

RudaBytecodeFunctionOptions ruda_bytecode_function_default_options(void);

const RudaOpcodeMetadata *ruda_opcode_metadata(RudaOpcode opcode);
const RudaOpcodeMetadata *ruda_opcode_metadata_from_wire(uint64_t wire_id);

bool ruda_bytecode_function_init(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaBytecodeType result_type,
    size_t source_count,
    const RudaBytecodeType *parameter_types,
    size_t parameter_count,
    RudaBytecodeFunctionOptions options
);
void ruda_bytecode_function_dispose(RudaBytecodeFunction *function);
bool ruda_bytecode_add_register(
    RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t *result
);
bool ruda_bytecode_add_block(
    RudaBytecodeFunction *function,
    uint32_t *result
);
bool ruda_bytecode_append_instruction(
    RudaBytecodeFunction *function,
    uint32_t block,
    const RudaBytecodeInstruction *instruction
);
bool ruda_bytecode_add_call(
    RudaBytecodeFunction *function,
    RudaSymbolId callee,
    RudaBytecodeType result_type,
    const RudaBytecodeCallArgument *arguments,
    size_t argument_count,
    RudaBytecodeCallId *result
);
const RudaBytecodeCall *ruda_bytecode_call(
    const RudaBytecodeFunction *function,
    RudaBytecodeCallId call
);
const RudaBytecodeCallArgument *ruda_bytecode_call_argument(
    const RudaBytecodeFunction *function,
    size_t index
);

RudaBytecodeVerifyStatus ruda_bytecode_verify(
    const RudaBytecodeFunction *function
);

/*
 * The internal function encoding is a bounded I3 bootstrap artifact, not the
 * complete public .rudab bundle contract. Output owns exact bytes on success.
 */
RudaBytecodeCodecStatus ruda_bytecode_encode_function(
    const RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaOwnedBytes *output
);

/*
 * Decodes canonical bytes and verifies the whole function before publishing
 * output. On failure output is reset and owns no allocations.
 */
RudaBytecodeCodecStatus ruda_bytecode_decode_function(
    const unsigned char *bytes,
    size_t length,
    RudaAllocator allocator,
    RudaBytecodeFunctionOptions options,
    RudaBytecodeFunction *output
);

RudaBytecodeDisassembleStatus ruda_bytecode_disassemble(
    const RudaBytecodeFunction *function,
    RudaWriter writer
);

#endif
