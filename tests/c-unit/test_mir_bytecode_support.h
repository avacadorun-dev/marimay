#ifndef MMAY_TEST_MIR_BYTECODE_SUPPORT_H
#define MMAY_TEST_MIR_BYTECODE_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/mir/mir.h"

MarimayMirInstruction mmay_test_mir_bytecode_instruction(
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
);
MarimayMirTerminator mmay_test_mir_bytecode_terminator(
    MarimayMirTerminatorKind kind,
    MarimayMirRegister value,
    MarimayMirBlockId target,
    MarimayMirBlockId otherwise,
    size_t start
);
MarimayMirBytecodeLowerOptions mmay_test_mir_bytecode_options(void);
const MarimayBytecodeInstruction *mmay_test_mir_bytecode_at(
    const MarimayBytecodeFunction *function,
    size_t block,
    size_t instruction
);

#endif
