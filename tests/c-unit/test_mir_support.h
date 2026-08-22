#ifndef MMAY_TEST_MIR_SUPPORT_H
#define MMAY_TEST_MIR_SUPPORT_H

#include "test_support.h"

#include <stddef.h>

#include "mmay/mir/mir.h"

MarimayMirInstruction mmay_test_mir_binary(
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second
);
MarimayMirTerminator mmay_test_mir_return(MarimayMirRegister value);
MarimayMirTerminator mmay_test_mir_goto(MarimayMirBlockId target);
MarimayMirTerminator mmay_test_mir_branch(
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false
);
MarimayMirTerminator mmay_test_mir_panic(
    MarimayMirRegister message,
    size_t start,
    size_t end
);
MarimayMirTerminator mmay_test_mir_call_never(
    MarimayMirCallId call,
    size_t start,
    size_t end
);

#endif
