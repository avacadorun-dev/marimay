#ifndef MMAY_MIR_CONTEXT_INTERNAL_H
#define MMAY_MIR_CONTEXT_INTERNAL_H

#include "mmay/mir/execution.h"
#include "mmay/mir/verify.h"

MarimayMirVerifyStatus mmay_mir_verify_context_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirInterpretStatus mmay_mir_execute_context_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
);

#endif
