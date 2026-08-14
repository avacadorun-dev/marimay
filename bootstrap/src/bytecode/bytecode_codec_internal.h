#ifndef RUDA_BYTECODE_CODEC_INTERNAL_H
#define RUDA_BYTECODE_CODEC_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/bytecode/bytecode.h"

enum {
    RUDA_BYTECODE_FEATURE_CALLS = 1U << 0,
    RUDA_BYTECODE_FEATURE_STRINGS = 1U << 1,
    RUDA_BYTECODE_FEATURE_F64 = 1U << 2,
    RUDA_BYTECODE_FEATURE_ARRAYS = 1U << 3,
    RUDA_BYTECODE_FEATURE_NOMINAL = 1U << 4
};

typedef struct RudaBytecodeEncodeBuffer {
    RudaVector bytes;
    size_t limit;
    bool limit_reached;
} RudaBytecodeEncodeBuffer;

typedef struct RudaBytecodeCursor {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
} RudaBytecodeCursor;

bool ruda_bytecode_codec_encode_append(
    RudaBytecodeEncodeBuffer *buffer,
    const void *bytes,
    size_t length
);
bool ruda_bytecode_codec_encode_u64(
    RudaBytecodeEncodeBuffer *buffer,
    uint64_t value
);
RudaBytecodeCodecStatus ruda_bytecode_codec_decode_u64(
    RudaBytecodeCursor *cursor,
    uint64_t *result
);
bool ruda_bytecode_codec_u64_to_size(uint64_t value, size_t *result);

bool ruda_bytecode_function_uses_nominal(
    const RudaBytecodeFunction *function
);
RudaBytecodeCodecStatus ruda_bytecode_encode_nominal_metadata(
    const RudaBytecodeFunction *function,
    RudaBytecodeEncodeBuffer *buffer
);
RudaBytecodeCodecStatus ruda_bytecode_decode_nominal_metadata(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
);

#endif
