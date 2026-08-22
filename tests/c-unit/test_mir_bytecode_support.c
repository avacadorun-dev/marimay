#include "test_mir_bytecode_support.h"

#include <string.h>

#include "mmay/mir/mir.h"

MarimayMirInstruction mmay_test_mir_bytecode_instruction(
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirRegister first,
    MarimayMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
) {
    MarimayMirInstruction instruction;
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

MarimayMirTerminator mmay_test_mir_bytecode_terminator(
    MarimayMirTerminatorKind kind,
    MarimayMirRegister value,
    MarimayMirBlockId target,
    MarimayMirBlockId otherwise,
    size_t start
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = kind;
    terminator.value = value;
    terminator.target = target;
    terminator.otherwise = otherwise;
    terminator.span.start = start;
    terminator.span.end = start + 1;
    return terminator;
}

MarimayMirBytecodeLowerOptions mmay_test_mir_bytecode_options(void) {
    MarimayMirBytecodeLowerOptions options =
        mmay_mir_bytecode_lower_default_options();
    options.source = 2;
    options.source_count = 3;
    return options;
}

const MarimayBytecodeInstruction *mmay_test_mir_bytecode_at(
    const MarimayBytecodeFunction *function,
    size_t block,
    size_t instruction
) {
    const MarimayBytecodeBlock *target = mmay_vector_at_const(
        &function->blocks,
        block
    );
    return target != NULL
        ? mmay_vector_at_const(&target->instructions, instruction)
        : NULL;
}

/* Builds a valid scalar CFG exercising every currently lowerable value opcode. */
