#ifndef MMAY_MIR_TEXT_INTERNAL_H
#define MMAY_MIR_TEXT_INTERNAL_H

#include "mmay/mir/mir.h"

MarimayMirVerifyStatus mmay_mir_verify_text_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirInterpretStatus mmay_mir_execute_text_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    const MarimayMirValue *third,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
);

#endif
