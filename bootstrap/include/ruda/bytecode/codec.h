#ifndef RUDA_BYTECODE_CODEC_H
#define RUDA_BYTECODE_CODEC_H

#include "ruda/base/writer.h"
#include "ruda/bytecode/function.h"

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

RudaBytecodeVerifyStatus ruda_bytecode_verify(
    const RudaBytecodeFunction *function
);

/* The function envelope is a bounded bootstrap artifact, not public .rudab. */
RudaBytecodeCodecStatus ruda_bytecode_encode_function(
    const RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaOwnedBytes *output
);

/* Failure resets output and transfers no allocations to the caller. */
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
