#ifndef MMAY_BYTECODE_CODEC_H
#define MMAY_BYTECODE_CODEC_H

#include "mmay/base/writer.h"
#include "mmay/bytecode/function.h"

typedef enum MarimayBytecodeVerifyStatus {
    MMAY_BYTECODE_VERIFY_OK,
    MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT,
    MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED,
    MMAY_BYTECODE_VERIFY_LIMIT_REACHED,
    MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW,
    MMAY_BYTECODE_VERIFY_INVALID_OPCODE,
    MMAY_BYTECODE_VERIFY_INVALID_REGISTER,
    MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER,
    MMAY_BYTECODE_VERIFY_TYPE_MISMATCH
} MarimayBytecodeVerifyStatus;

typedef enum MarimayBytecodeCodecStatus {
    MMAY_BYTECODE_CODEC_OK,
    MMAY_BYTECODE_CODEC_INVALID_ARGUMENT,
    MMAY_BYTECODE_CODEC_ALLOCATION_FAILED,
    MMAY_BYTECODE_CODEC_LIMIT_REACHED,
    MMAY_BYTECODE_CODEC_TRUNCATED,
    MMAY_BYTECODE_CODEC_INVALID_ENCODING,
    MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION,
    MMAY_BYTECODE_CODEC_VERIFY_FAILED,
    MMAY_BYTECODE_CODEC_TRAILING_DATA
} MarimayBytecodeCodecStatus;

typedef enum MarimayBytecodeDisassembleStatus {
    MMAY_BYTECODE_DISASSEMBLE_OK,
    MMAY_BYTECODE_DISASSEMBLE_INVALID_ARGUMENT,
    MMAY_BYTECODE_DISASSEMBLE_INVALID_BYTECODE,
    MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED
} MarimayBytecodeDisassembleStatus;

MarimayBytecodeVerifyStatus mmay_bytecode_verify(
    const MarimayBytecodeFunction *function
);

/* The function envelope is a bounded bootstrap artifact, not public .myb. */
MarimayBytecodeCodecStatus mmay_bytecode_encode_function(
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayOwnedBytes *output
);

/* Failure resets output and transfers no allocations to the caller. */
MarimayBytecodeCodecStatus mmay_bytecode_decode_function(
    const unsigned char *bytes,
    size_t length,
    MarimayAllocator allocator,
    MarimayBytecodeFunctionOptions options,
    MarimayBytecodeFunction *output
);

MarimayBytecodeDisassembleStatus mmay_bytecode_disassemble(
    const MarimayBytecodeFunction *function,
    MarimayWriter writer
);

#endif
