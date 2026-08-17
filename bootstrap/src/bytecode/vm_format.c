#include "vm_internal.h"

#include "ruda/base/number_format.h"

RudaBytecodeRunStatus ruda_bytecode_run_format_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution
) {
    uint32_t target = (uint32_t)instruction->operands[0].unsigned_value;
    uint32_t source = (uint32_t)instruction->operands[1].unsigned_value;
    char text[RUDA_NUMBER_FORMAT_F64_CAPACITY];
    size_t length = 0;
    RudaNumberFormatStatus status = instruction->opcode == RUDA_OPCODE_FORMAT_F32
        ? ruda_format_f32(
            (uint32_t)registers[source].as.integer_bits,
            text,
            sizeof(text),
            &length
        )
        : ruda_format_f64(
            registers[source].as.f64_bits,
            text,
            sizeof(text),
            &length
        );
    if (status != RUDA_NUMBER_FORMAT_OK) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return ruda_bytecode_execution_intern(
        execution,
        (const unsigned char *)text,
        length,
        &registers[target].as.reference
    );
}
