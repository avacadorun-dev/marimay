#ifndef MMAY_BYTECODE_CODEC_INTERNAL_H
#define MMAY_BYTECODE_CODEC_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/bytecode/bytecode.h"

enum {
    MMAY_BYTECODE_FEATURE_CALLS = 1U << 0,
    MMAY_BYTECODE_FEATURE_STRINGS = 1U << 1,
    MMAY_BYTECODE_FEATURE_F64 = 1U << 2,
    MMAY_BYTECODE_FEATURE_ARRAYS = 1U << 3,
    MMAY_BYTECODE_FEATURE_NOMINAL = 1U << 4,
    /* The extended envelope carries canonical []T descriptors. */
    MMAY_BYTECODE_FEATURE_ARRAY_TYPES = 1U << 5,
    MMAY_BYTECODE_FEATURE_COLLECTIONS = 1U << 6
};

typedef struct MarimayBytecodeEncodeBuffer {
    MarimayVector bytes;
    size_t limit;
    bool limit_reached;
} MarimayBytecodeEncodeBuffer;

typedef struct MarimayBytecodeCursor {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
} MarimayBytecodeCursor;

bool mmay_bytecode_codec_encode_append(
    MarimayBytecodeEncodeBuffer *buffer,
    const void *bytes,
    size_t length
);
bool mmay_bytecode_codec_encode_u64(
    MarimayBytecodeEncodeBuffer *buffer,
    uint64_t value
);
MarimayBytecodeCodecStatus mmay_bytecode_codec_decode_u64(
    MarimayBytecodeCursor *cursor,
    uint64_t *result
);
bool mmay_bytecode_codec_u64_to_size(uint64_t value, size_t *result);

bool mmay_bytecode_function_uses_nominal(
    const MarimayBytecodeFunction *function
);
MarimayBytecodeCodecStatus mmay_bytecode_encode_nominal_metadata(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEncodeBuffer *buffer
);
MarimayBytecodeCodecStatus mmay_bytecode_decode_nominal_metadata(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
);

#endif
