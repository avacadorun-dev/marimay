#ifndef RUDA_BYTECODE_CODEC_NOMINAL_VALUES_H
#define RUDA_BYTECODE_CODEC_NOMINAL_VALUES_H

#include "bytecode_codec_internal.h"

/* Canonical function-result, register, and call layout metadata owner. */
bool ruda_bytecode_nominal_values_encode(
    const RudaBytecodeFunction *function,
    RudaBytecodeEncodeBuffer *buffer
);
RudaBytecodeCodecStatus ruda_bytecode_nominal_values_decode(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
);

#endif
