#include "bytecode_internal.h"

static const MarimayBytecodeType *mmay_bytecode_text_register(
    const MarimayBytecodeFunction *function,
    uint64_t register_id
) {
    return function != NULL && register_id < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)register_id)
        : NULL;
}

static bool mmay_bytecode_text_array(
    const MarimayBytecodeFunction *function,
    uint64_t register_id,
    MarimayBytecodeType element
) {
    const MarimayBytecodeType *type = mmay_bytecode_text_register(
        function, register_id
    );
    uint32_t ignored = MMAY_BYTECODE_NO_LAYOUT;
    return type != NULL && mmay_bytecode_type_is_array(*type)
        && mmay_bytecode_function_array_element_type(
            function, *type,
            mmay_bytecode_register_layout(function, (uint32_t)register_id),
            &ignored
        ) == element;
}

bool mmay_bytecode_opcode_is_text(MarimayOpcode opcode) {
    return opcode >= MMAY_OPCODE_TEXT_BYTE_LENGTH
        && opcode <= MMAY_OPCODE_TEXT_DECODE_UTF8;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_text_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = instruction->operands[1].unsigned_value;
    uint64_t second = instruction->operands[2].unsigned_value;
    const MarimayBytecodeType *target_type = mmay_bytecode_text_register(
        function, target
    );
    const MarimayBytecodeType *first_type = mmay_bytecode_text_register(
        function, first
    );
    const MarimayBytecodeType *second_type = NULL;
    bool binary = instruction->opcode == MMAY_OPCODE_TEXT_IS_RUNE_BOUNDARY
        || (instruction->opcode >= MMAY_OPCODE_TEXT_DROP_PREFIX
            && instruction->opcode <= MMAY_OPCODE_TEXT_ENDS_WITH)
        || instruction->opcode == MMAY_OPCODE_TEXT_SPLIT_COUNT
        || instruction->opcode == MMAY_OPCODE_TEXT_SPLIT
        || instruction->opcode == MMAY_OPCODE_TEXT_JOIN;
    if (target_type == NULL || first_type == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    if (assigned[first] == 0) {
        return MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
    }
    if (binary) {
        second_type = mmay_bytecode_text_register(function, second);
        if (second_type == NULL) return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
        if (assigned[second] == 0) {
            return MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
        }
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_TEXT_BYTE_LENGTH:
        case MMAY_OPCODE_TEXT_RUNE_COUNT:
        case MMAY_OPCODE_TEXT_GRAPHEME_COUNT:
            return *target_type == MMAY_BYTECODE_TYPE_I64
                    && *first_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_IS_RUNE_BOUNDARY:
            return *target_type == MMAY_BYTECODE_TYPE_BOOL
                    && *first_type == MMAY_BYTECODE_TYPE_STRING
                    && *second_type == MMAY_BYTECODE_TYPE_I64
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_DROP_PREFIX:
        case MMAY_OPCODE_TEXT_TAKE_PREFIX:
            return *target_type == MMAY_BYTECODE_TYPE_STRING
                    && *first_type == MMAY_BYTECODE_TYPE_STRING
                    && *second_type == MMAY_BYTECODE_TYPE_I64
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_CONTAINS:
        case MMAY_OPCODE_TEXT_STARTS_WITH:
        case MMAY_OPCODE_TEXT_ENDS_WITH:
            return *target_type == MMAY_BYTECODE_TYPE_BOOL
                    && *first_type == MMAY_BYTECODE_TYPE_STRING
                    && *second_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_TRIM_ASCII:
            return *target_type == MMAY_BYTECODE_TYPE_STRING
                    && *first_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_SPLIT_COUNT:
            return *target_type == MMAY_BYTECODE_TYPE_I64
                    && *first_type == MMAY_BYTECODE_TYPE_STRING
                    && *second_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_SPLIT:
            return mmay_bytecode_text_array(
                    function, target, MMAY_BYTECODE_TYPE_STRING
                ) && *first_type == MMAY_BYTECODE_TYPE_STRING
                    && *second_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_JOIN:
            return *target_type == MMAY_BYTECODE_TYPE_STRING
                    && mmay_bytecode_text_array(
                        function, first, MMAY_BYTECODE_TYPE_STRING
                    ) && *second_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_RUNES:
            return mmay_bytecode_text_array(
                    function, target, MMAY_BYTECODE_TYPE_RUNE
                ) && *first_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_GRAPHEMES:
            return mmay_bytecode_text_array(
                    function, target, MMAY_BYTECODE_TYPE_STRING
                ) && *first_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_ENCODE_UTF8:
            return mmay_bytecode_text_array(
                    function, target, MMAY_BYTECODE_TYPE_U8
                ) && *first_type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_INVALID_UTF8_OFFSET:
            return *target_type == MMAY_BYTECODE_TYPE_I64
                    && mmay_bytecode_text_array(
                        function, first, MMAY_BYTECODE_TYPE_U8
                    )
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_TEXT_DECODE_UTF8:
            return *target_type == MMAY_BYTECODE_TYPE_STRING
                    && mmay_bytecode_text_array(
                        function, first, MMAY_BYTECODE_TYPE_U8
                    )
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        default: return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
    }
}
