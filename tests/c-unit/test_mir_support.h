#ifndef RUDA_TEST_MIR_SUPPORT_H
#define RUDA_TEST_MIR_SUPPORT_H

#include "test_support.h"

#include <stddef.h>

#include "ruda/mir/mir.h"

RudaMirInstruction ruda_test_mir_binary(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second
);
RudaMirTerminator ruda_test_mir_return(RudaMirRegister value);
RudaMirTerminator ruda_test_mir_goto(RudaMirBlockId target);
RudaMirTerminator ruda_test_mir_branch(
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false
);
RudaMirTerminator ruda_test_mir_panic(
    RudaMirRegister message,
    size_t start,
    size_t end
);
RudaMirTerminator ruda_test_mir_call_never(
    RudaMirCallId call,
    size_t start,
    size_t end
);

#endif
