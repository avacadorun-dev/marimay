#include "test_mir_support.h"

#include <string.h>

RudaMirInstruction ruda_test_mir_binary(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    return instruction;
}

RudaMirTerminator ruda_test_mir_return(RudaMirRegister value) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_RETURN;
    terminator.value = value;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    return terminator;
}

RudaMirTerminator ruda_test_mir_goto(RudaMirBlockId target) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_GOTO;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    return terminator;
}

RudaMirTerminator ruda_test_mir_branch(
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    return terminator;
}

RudaMirTerminator ruda_test_mir_panic(
    RudaMirRegister message,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_PANIC;
    terminator.value = message;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    return terminator;
}

RudaMirTerminator ruda_test_mir_call_never(
    RudaMirCallId call,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_CALL_NEVER;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.call = call;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    return terminator;
}
