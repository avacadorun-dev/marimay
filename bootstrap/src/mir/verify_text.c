#include "mir_internal.h"
#include "text_internal.h"

static bool mmay_mir_text_array(
    const MarimayMirFunction *function,
    MarimayMirArrayId id,
    MarimayTypeId type,
    MarimayTypeId element
) {
    const MarimayMirArray *array = mmay_mir_array(function, id);
    return array != NULL && array->type == type
        && array->element_type == element;
}

MarimayMirVerifyStatus mmay_mir_verify_text_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId target = mmay_mir_register_type(
        function, instruction->target
    );
    MarimayTypeId first = mmay_mir_register_type(
        function, instruction->first
    );
    MarimayTypeId second = instruction->kind == MMAY_MIR_TEXT_BINARY
        ? mmay_mir_register_type(function, instruction->second)
        : MMAY_TYPE_INVALID;
    if (target == MMAY_TYPE_INVALID || first == MMAY_TYPE_INVALID
        || (instruction->kind == MMAY_MIR_TEXT_BINARY
            && second == MMAY_TYPE_INVALID)) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->text_operation) {
        case MMAY_MIR_TEXT_BYTE_LENGTH:
        case MMAY_MIR_TEXT_RUNE_COUNT:
        case MMAY_MIR_TEXT_GRAPHEME_COUNT:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && target == MMAY_TYPE_I64 && first == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_IS_RUNE_BOUNDARY:
            return instruction->kind == MMAY_MIR_TEXT_BINARY
                    && target == MMAY_TYPE_BOOL && first == MMAY_TYPE_STRING
                    && second == MMAY_TYPE_I64
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_DROP_PREFIX:
        case MMAY_MIR_TEXT_TAKE_PREFIX:
            return instruction->kind == MMAY_MIR_TEXT_BINARY
                    && target == MMAY_TYPE_STRING && first == MMAY_TYPE_STRING
                    && second == MMAY_TYPE_I64
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_CONTAINS:
        case MMAY_MIR_TEXT_STARTS_WITH:
        case MMAY_MIR_TEXT_ENDS_WITH:
            return instruction->kind == MMAY_MIR_TEXT_BINARY
                    && target == MMAY_TYPE_BOOL && first == MMAY_TYPE_STRING
                    && second == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_TRIM_ASCII:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && target == MMAY_TYPE_STRING && first == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_SPLIT_COUNT:
            return instruction->kind == MMAY_MIR_TEXT_BINARY
                    && target == MMAY_TYPE_I64 && first == MMAY_TYPE_STRING
                    && second == MMAY_TYPE_STRING
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_SPLIT:
            return instruction->kind == MMAY_MIR_TEXT_BINARY
                    && first == MMAY_TYPE_STRING && second == MMAY_TYPE_STRING
                    && mmay_mir_text_array(
                        function, instruction->array, target, MMAY_TYPE_STRING
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_JOIN:
            return instruction->kind == MMAY_MIR_TEXT_BINARY
                    && target == MMAY_TYPE_STRING
                    && second == MMAY_TYPE_STRING
                    && mmay_mir_text_array(
                        function, instruction->array, first, MMAY_TYPE_STRING
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_RUNES:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && first == MMAY_TYPE_STRING
                    && mmay_mir_text_array(
                        function, instruction->array, target, MMAY_TYPE_RUNE
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_GRAPHEMES:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && first == MMAY_TYPE_STRING
                    && mmay_mir_text_array(
                        function, instruction->array, target, MMAY_TYPE_STRING
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_ENCODE_UTF8:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && first == MMAY_TYPE_STRING
                    && mmay_mir_text_array(
                        function, instruction->array, target, MMAY_TYPE_U8
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_INVALID_UTF8_OFFSET:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && target == MMAY_TYPE_I64
                    && mmay_mir_text_array(
                        function, instruction->array, first, MMAY_TYPE_U8
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_TEXT_DECODE_UTF8:
            return instruction->kind == MMAY_MIR_TEXT_UNARY
                    && target == MMAY_TYPE_STRING
                    && mmay_mir_text_array(
                        function, instruction->array, first, MMAY_TYPE_U8
                    )
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
}
