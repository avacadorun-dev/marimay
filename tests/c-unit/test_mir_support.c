#include "test_mir_support.h"

#include <string.h>

MarimayMirInstruction mmay_test_mir_binary(
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    return instruction;
}

MarimayMirTerminator mmay_test_mir_return(MarimayMirRegister value) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_RETURN;
    terminator.value = value;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    return terminator;
}

MarimayMirTerminator mmay_test_mir_goto(MarimayMirBlockId target) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_GOTO;
    terminator.value = MMAY_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    return terminator;
}

MarimayMirTerminator mmay_test_mir_branch(
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    return terminator;
}

MarimayMirTerminator mmay_test_mir_panic(
    MarimayMirRegister message,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_PANIC;
    terminator.value = message;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    return terminator;
}

MarimayMirTerminator mmay_test_mir_call_never(
    MarimayMirCallId call,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_CALL_NEVER;
    terminator.value = MMAY_MIR_NO_REGISTER;
    terminator.call = call;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    return terminator;
}
