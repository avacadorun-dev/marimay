#ifndef RUDA_BYTECODE_MIR_LOWER_INTERNAL_H
#define RUDA_BYTECODE_MIR_LOWER_INTERNAL_H

#include "ruda/bytecode/mir_lower.h"

bool ruda_mir_bytecode_type(
    const RudaMirFunction *function,
    RudaTypeId input,
    bool allow_void,
    bool allow_never,
    RudaBytecodeType *output
);

uint32_t ruda_mir_bytecode_layout_for_type(
    const RudaMirFunction *function,
    RudaTypeId type
);

RudaMirBytecodeLowerStatus ruda_mir_bytecode_lower_nominals(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output,
    RudaAllocator allocator
);

#endif
