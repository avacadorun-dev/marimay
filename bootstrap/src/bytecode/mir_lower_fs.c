#include "mir_lower_internal.h"

static MarimayOpcode mmay_mir_fs_opcode(MarimayMirFsOperation operation) {
    switch (operation) {
        case MMAY_MIR_PATH_NORMALIZE: return MMAY_OPCODE_PATH_NORMALIZE;
        case MMAY_MIR_PATH_IS_ABSOLUTE: return MMAY_OPCODE_PATH_IS_ABSOLUTE;
        case MMAY_MIR_FS_OPEN_READ: return MMAY_OPCODE_FS_OPEN_READ;
        case MMAY_MIR_FS_READ: return MMAY_OPCODE_FS_READ;
        case MMAY_MIR_FS_READ_LINE: return MMAY_OPCODE_FS_READ_LINE;
        case MMAY_MIR_FS_CLOSE: return MMAY_OPCODE_FS_CLOSE;
        case MMAY_MIR_FS_CLOSE_BEST_EFFORT:
            return MMAY_OPCODE_FS_CLOSE_BEST_EFFORT;
        case MMAY_MIR_FS_READ_ALL: return MMAY_OPCODE_FS_READ_ALL;
        case MMAY_MIR_FS_WRITE_ATOMIC: return MMAY_OPCODE_FS_WRITE_ATOMIC;
        case MMAY_MIR_FS_METADATA: return MMAY_OPCODE_FS_METADATA;
        case MMAY_MIR_FS_READ_DIRECTORY:
            return MMAY_OPCODE_FS_READ_DIRECTORY;
    }
    return MMAY_OPCODE_INVALID;
}

MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_fs_instruction(
    const MarimayMirInstruction *input,
    MarimayBytecodeInstruction *output,
    bool *handled
) {
    if (input == NULL || output == NULL || handled == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    *handled = input->kind == MMAY_MIR_PATH
        || input->kind == MMAY_MIR_FS_UNARY
        || input->kind == MMAY_MIR_FS_BINARY;
    if (!*handled) return MMAY_MIR_BYTECODE_LOWER_OK;
    output->opcode = mmay_mir_fs_opcode(input->fs_operation);
    if (output->opcode == MMAY_OPCODE_INVALID) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
    }
    output->operands[1].unsigned_value = input->first;
    if (input->kind == MMAY_MIR_FS_BINARY) {
        output->operands[2].unsigned_value = input->second;
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}
