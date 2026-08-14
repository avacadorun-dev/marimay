#include "test_mir_bytecode_support.h"

#include <string.h>

#include "ruda/mir/mir.h"

RudaMirInstruction ruda_test_mir_bytecode_instruction(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.integer = integer;
    instruction.boolean = boolean;
    instruction.span.start = start;
    instruction.span.end = start + 1;
    return instruction;
}

RudaMirTerminator ruda_test_mir_bytecode_terminator(
    RudaMirTerminatorKind kind,
    RudaMirRegister value,
    RudaMirBlockId target,
    RudaMirBlockId otherwise,
    size_t start
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = kind;
    terminator.value = value;
    terminator.target = target;
    terminator.otherwise = otherwise;
    terminator.span.start = start;
    terminator.span.end = start + 1;
    return terminator;
}

RudaMirBytecodeLowerOptions ruda_test_mir_bytecode_options(void) {
    RudaMirBytecodeLowerOptions options =
        ruda_mir_bytecode_lower_default_options();
    options.source = 2;
    options.source_count = 3;
    return options;
}

const RudaBytecodeInstruction *ruda_test_mir_bytecode_at(
    const RudaBytecodeFunction *function,
    size_t block,
    size_t instruction
) {
    const RudaBytecodeBlock *target = ruda_vector_at_const(
        &function->blocks,
        block
    );
    return target != NULL
        ? ruda_vector_at_const(&target->instructions, instruction)
        : NULL;
}

/* Builds a valid scalar CFG exercising every currently lowerable value opcode. */
