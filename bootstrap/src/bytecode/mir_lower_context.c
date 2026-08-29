#include "mir_lower_internal.h"

MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_context_instruction(
    const MarimayMirInstruction *input,
    MarimayBytecodeInstruction *output,
    bool *handled
) {
    if (input == NULL || output == NULL || handled == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    *handled = input->kind == MMAY_MIR_ENV_READ
        || input->kind == MMAY_MIR_PROCESS_CONTEXT;
    if (!*handled) return MMAY_MIR_BYTECODE_LOWER_OK;
    if (input->kind == MMAY_MIR_ENV_READ
        && input->context_operation == MMAY_MIR_CONTEXT_ENV_GET) {
        output->opcode = MMAY_OPCODE_ENV_GET;
        output->operands[1].unsigned_value = input->first;
        return MMAY_MIR_BYTECODE_LOWER_OK;
    }
    if (input->kind == MMAY_MIR_PROCESS_CONTEXT) {
        if (input->context_operation == MMAY_MIR_CONTEXT_CURRENT_DIRECTORY) {
            output->opcode = MMAY_OPCODE_PROCESS_CURRENT_DIRECTORY;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        }
        if (input->context_operation == MMAY_MIR_CONTEXT_EXECUTABLE_PATH) {
            output->opcode = MMAY_OPCODE_PROCESS_EXECUTABLE_PATH;
            return MMAY_MIR_BYTECODE_LOWER_OK;
        }
    }
    return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
}
