#ifndef MMAY_BYTECODE_CODEC_NOMINAL_VALUES_H
#define MMAY_BYTECODE_CODEC_NOMINAL_VALUES_H

#include "bytecode_codec_internal.h"

/* Canonical function-result, register, and call layout metadata owner. */
bool mmay_bytecode_nominal_values_encode(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEncodeBuffer *buffer
);
MarimayBytecodeCodecStatus mmay_bytecode_nominal_values_decode(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
);

#endif
