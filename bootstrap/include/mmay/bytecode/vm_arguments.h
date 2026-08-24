#ifndef MMAY_BYTECODE_VM_ARGUMENTS_H
#define MMAY_BYTECODE_VM_ARGUMENTS_H

#include "mmay/bytecode/vm.h"
#include "mmay/runtime/program_arguments.h"

MarimayBytecodeRunStatus mmay_bytecode_execution_program_arguments(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *entry,
    MarimayProgramArgumentList arguments,
    MarimayBytecodeValue *result
);

#endif
