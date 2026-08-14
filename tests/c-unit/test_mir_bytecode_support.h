#ifndef RUDA_TEST_MIR_BYTECODE_SUPPORT_H
#define RUDA_TEST_MIR_BYTECODE_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/vm.h"
#include "ruda/mir/mir.h"

RudaMirInstruction ruda_test_mir_bytecode_instruction(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
);
RudaMirTerminator ruda_test_mir_bytecode_terminator(
    RudaMirTerminatorKind kind,
    RudaMirRegister value,
    RudaMirBlockId target,
    RudaMirBlockId otherwise,
    size_t start
);
RudaMirBytecodeLowerOptions ruda_test_mir_bytecode_options(void);
const RudaBytecodeInstruction *ruda_test_mir_bytecode_at(
    const RudaBytecodeFunction *function,
    size_t block,
    size_t instruction
);

#endif
