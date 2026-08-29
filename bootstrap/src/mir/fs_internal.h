#ifndef MMAY_MIR_FS_INTERNAL_H
#define MMAY_MIR_FS_INTERNAL_H

#include "mmay/mir/execution.h"
#include "mmay/mir/verify.h"

MarimayMirVerifyStatus mmay_mir_verify_fs_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirInterpretStatus mmay_mir_execute_fs_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
);

#endif
