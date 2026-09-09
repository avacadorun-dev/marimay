#include "mir_lower_internal.h"

static MarimayOpcode mmay_mir_text_opcode(MarimayMirTextOperation operation) {
    switch (operation) {
        case MMAY_MIR_TEXT_SUBSTRING: return MMAY_OPCODE_TEXT_SUBSTRING;
        case MMAY_MIR_TEXT_BYTE_LENGTH: return MMAY_OPCODE_TEXT_BYTE_LENGTH;
        case MMAY_MIR_TEXT_RUNE_COUNT: return MMAY_OPCODE_TEXT_RUNE_COUNT;
        case MMAY_MIR_TEXT_GRAPHEME_COUNT:
            return MMAY_OPCODE_TEXT_GRAPHEME_COUNT;
        case MMAY_MIR_TEXT_IS_RUNE_BOUNDARY:
            return MMAY_OPCODE_TEXT_IS_RUNE_BOUNDARY;
        case MMAY_MIR_TEXT_DROP_PREFIX: return MMAY_OPCODE_TEXT_DROP_PREFIX;
        case MMAY_MIR_TEXT_TAKE_PREFIX: return MMAY_OPCODE_TEXT_TAKE_PREFIX;
        case MMAY_MIR_TEXT_CONTAINS: return MMAY_OPCODE_TEXT_CONTAINS;
        case MMAY_MIR_TEXT_STARTS_WITH:
            return MMAY_OPCODE_TEXT_STARTS_WITH;
        case MMAY_MIR_TEXT_ENDS_WITH: return MMAY_OPCODE_TEXT_ENDS_WITH;
        case MMAY_MIR_TEXT_TRIM_ASCII: return MMAY_OPCODE_TEXT_TRIM_ASCII;
        case MMAY_MIR_TEXT_SPLIT_COUNT: return MMAY_OPCODE_TEXT_SPLIT_COUNT;
        case MMAY_MIR_TEXT_SPLIT: return MMAY_OPCODE_TEXT_SPLIT;
        case MMAY_MIR_TEXT_JOIN: return MMAY_OPCODE_TEXT_JOIN;
        case MMAY_MIR_TEXT_RUNES: return MMAY_OPCODE_TEXT_RUNES;
        case MMAY_MIR_TEXT_GRAPHEMES: return MMAY_OPCODE_TEXT_GRAPHEMES;
        case MMAY_MIR_TEXT_ENCODE_UTF8: return MMAY_OPCODE_TEXT_ENCODE_UTF8;
        case MMAY_MIR_TEXT_INVALID_UTF8_OFFSET:
            return MMAY_OPCODE_TEXT_INVALID_UTF8_OFFSET;
        case MMAY_MIR_TEXT_DECODE_UTF8:
            return MMAY_OPCODE_TEXT_DECODE_UTF8;
    }
    return MMAY_OPCODE_INVALID;
}

MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_text_instruction(
    const MarimayMirInstruction *input,
    MarimayBytecodeInstruction *output,
    bool *handled
) {
    if (input == NULL || output == NULL || handled == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    *handled = true;
    if (input->kind == MMAY_MIR_TEXT_UNARY
        || input->kind == MMAY_MIR_TEXT_BINARY
        || input->kind == MMAY_MIR_TEXT_TERNARY) {
        output->opcode = mmay_mir_text_opcode(input->text_operation);
        if (output->opcode == MMAY_OPCODE_INVALID) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        output->operands[1].unsigned_value = input->first;
        if (input->kind != MMAY_MIR_TEXT_UNARY) {
            output->operands[2].unsigned_value = input->second;
        }
        if (input->kind == MMAY_MIR_TEXT_TERNARY) {
            output->operands[3].unsigned_value = input->third;
        }
        return MMAY_MIR_BYTECODE_LOWER_OK;
    }
    if (input->kind == MMAY_MIR_IO_READ_BYTES) {
        output->opcode = MMAY_OPCODE_IO_READ_BYTES;
        output->operands[1].unsigned_value = input->first;
        output->operands[2].signed_value = (int64_t)input->array;
        return MMAY_MIR_BYTECODE_LOWER_OK;
    }
    if (input->kind == MMAY_MIR_IO_WRITE_BYTES) {
        output->opcode = MMAY_OPCODE_IO_WRITE_BYTES;
        output->operands[1].unsigned_value = input->first;
        output->operands[2].signed_value = (int64_t)input->io_operation;
        return MMAY_MIR_BYTECODE_LOWER_OK;
    }
    *handled = false;
    return MMAY_MIR_BYTECODE_LOWER_OK;
}
