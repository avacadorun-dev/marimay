#include "mir_lower_internal.h"

MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_format_instruction(
    const MarimayMirInstruction *input,
    MarimayBytecodeInstruction *output,
    bool *handled
) {
    if (input == NULL || output == NULL || handled == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    *handled = true;
    switch (input->kind) {
        case MMAY_MIR_FORMAT_F32: output->opcode = MMAY_OPCODE_FORMAT_F32; break;
        case MMAY_MIR_FORMAT_F64: output->opcode = MMAY_OPCODE_FORMAT_F64; break;
        case MMAY_MIR_FORMAT_BOOL: output->opcode = MMAY_OPCODE_FORMAT_BOOL; break;
        case MMAY_MIR_FORMAT_INTEGER:
            output->opcode = MMAY_OPCODE_FORMAT_INTEGER;
            break;
        case MMAY_MIR_STRING_CONCAT:
            output->opcode = MMAY_OPCODE_STRING_CONCAT;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        case MMAY_MIR_PARSE_INTEGER:
            output->opcode = MMAY_OPCODE_PARSE_INTEGER;
            output->operands[1].unsigned_value = input->first;
            output->operands[2].unsigned_value = input->second;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        default:
            *handled = false;
            return MMAY_MIR_BYTECODE_LOWER_OK;
    }
    output->operands[1].unsigned_value = input->first;
    return MMAY_MIR_BYTECODE_LOWER_OK;
}
