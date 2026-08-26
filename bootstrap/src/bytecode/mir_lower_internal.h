#ifndef MMAY_BYTECODE_MIR_LOWER_INTERNAL_H
#define MMAY_BYTECODE_MIR_LOWER_INTERNAL_H

#include "mmay/bytecode/mir_lower.h"

bool mmay_mir_bytecode_type(
    const MarimayMirFunction *function,
    MarimayTypeId input,
    bool allow_void,
    bool allow_never,
    MarimayBytecodeType *output
);

uint32_t mmay_mir_bytecode_layout_for_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
);

bool mmay_bytecode_numeric_option_symbol(
    MarimayBytecodeType payload_type,
    MarimaySymbolId *symbol
);

MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_nominals(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output,
    MarimayAllocator allocator
);
MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_format_instruction(
    const MarimayMirInstruction *input,
    MarimayBytecodeInstruction *output,
    bool *handled
);
MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_text_instruction(
    const MarimayMirInstruction *input,
    MarimayBytecodeInstruction *output,
    bool *handled
);

#endif
