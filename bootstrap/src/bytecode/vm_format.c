#include "vm_internal.h"

#include "mmay/base/number_format.h"

MarimayBytecodeRunStatus mmay_bytecode_run_format_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution
) {
    uint32_t target = (uint32_t)instruction->operands[0].unsigned_value;
    uint32_t source = (uint32_t)instruction->operands[1].unsigned_value;
    char text[MMAY_NUMBER_FORMAT_F64_CAPACITY];
    size_t length = 0;
    MarimayNumberFormatStatus status = instruction->opcode == MMAY_OPCODE_FORMAT_F32
        ? mmay_format_f32(
            (uint32_t)registers[source].as.integer_bits,
            text,
            sizeof(text),
            &length
        )
        : mmay_format_f64(
            registers[source].as.f64_bits,
            text,
            sizeof(text),
            &length
        );
    if (status != MMAY_NUMBER_FORMAT_OK) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return mmay_bytecode_execution_intern(
        execution,
        (const unsigned char *)text,
        length,
        &registers[target].as.reference
    );
}
