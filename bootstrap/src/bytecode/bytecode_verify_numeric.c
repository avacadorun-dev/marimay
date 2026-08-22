#include "bytecode_internal.h"

#include <string.h>

bool mmay_bytecode_opcode_is_numeric(MarimayOpcode opcode) {
    return (opcode >= MMAY_OPCODE_INTEGER_CHECKED_ADD
            && opcode <= MMAY_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT)
        || opcode == MMAY_OPCODE_INTEGER_CONVERT_TOTAL
        || opcode == MMAY_OPCODE_INTEGER_CONVERT_OPTION;
}

bool mmay_bytecode_numeric_option_symbol(
    MarimayBytecodeType payload_type,
    MarimaySymbolId *symbol
) {
    if (symbol == NULL || !mmay_bytecode_type_is_integer(payload_type)) {
        return false;
    }
    return mmay_symbol_id_from_bytes(
        "std",
        3,
        "prelude",
        7,
        (const unsigned char *)"Option",
        6,
        UINT32_C(0x4f000000) | (uint32_t)payload_type,
        symbol
    );
}

static bool mmay_bytecode_numeric_signed(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_I8
        || type == MMAY_BYTECODE_TYPE_I16
        || type == MMAY_BYTECODE_TYPE_I32
        || type == MMAY_BYTECODE_TYPE_I64;
}

static unsigned int mmay_bytecode_numeric_width(MarimayBytecodeType type) {
    switch (type) {
        case MMAY_BYTECODE_TYPE_I8: case MMAY_BYTECODE_TYPE_U8: return 8;
        case MMAY_BYTECODE_TYPE_I16: case MMAY_BYTECODE_TYPE_U16: return 16;
        case MMAY_BYTECODE_TYPE_I32: case MMAY_BYTECODE_TYPE_U32: return 32;
        case MMAY_BYTECODE_TYPE_I64: case MMAY_BYTECODE_TYPE_U64: return 64;
        default: return 0;
    }
}

static bool mmay_bytecode_numeric_range_preserving(
    MarimayBytecodeType source,
    MarimayBytecodeType target
) {
    unsigned int source_width = mmay_bytecode_numeric_width(source);
    unsigned int target_width = mmay_bytecode_numeric_width(target);
    bool source_signed = mmay_bytecode_numeric_signed(source);
    bool target_signed = mmay_bytecode_numeric_signed(target);
    if (source_width == 0 || target_width == 0) {
        return false;
    }
    if (source_signed == target_signed) {
        return source_width <= target_width;
    }
    return !source_signed && target_signed && source_width < target_width;
}

static bool mmay_bytecode_numeric_option_payload(
    const MarimayBytecodeFunction *function,
    uint64_t target,
    MarimayBytecodeType *payload_type
) {
    MarimaySymbolId option_symbol;
    uint32_t layout_id = target <= UINT32_MAX
        ? mmay_bytecode_register_layout(function, (uint32_t)target)
        : MMAY_BYTECODE_NO_LAYOUT;
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, layout_id
    );
    const MarimayBytecodeEnumVariant *some = layout != NULL
            && layout->variant_count == 2
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start
        ) : NULL;
    const MarimayBytecodeEnumVariant *none = layout != NULL
            && layout->variant_count == 2
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + 1
        ) : NULL;
    const MarimayBytecodeEnumPayload *payload = some != NULL
            && some->payload_count == 1
        ? mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + some->payload_start
        ) : NULL;
    if (payload_type == NULL || layout == NULL
        || layout->payload_count != 1 || some == NULL
        || some->payload_start != 0 || some->payload_count != 1
        || none == NULL || none->payload_count != 0 || payload == NULL
        || payload->layout != MMAY_BYTECODE_NO_LAYOUT
        || !mmay_bytecode_type_is_integer(payload->type)
        || !mmay_bytecode_numeric_option_symbol(
            payload->type, &option_symbol
        ) || memcmp(layout->symbol.bytes, option_symbol.bytes, 16) != 0) {
        return false;
    }
    *payload_type = payload->type;
    return true;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_numeric_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = instruction->operands[1].unsigned_value;
    uint64_t second = instruction->operands[2].unsigned_value;
    const MarimayBytecodeType *target_type = target < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)target)
        : NULL;
    const MarimayBytecodeType *first_type = first < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)first)
        : NULL;
    bool checked = instruction->opcode >= MMAY_OPCODE_INTEGER_CHECKED_ADD
        && instruction->opcode <= MMAY_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT;
    bool unary = instruction->opcode == MMAY_OPCODE_INTEGER_CHECKED_NEGATE
        || instruction->opcode == MMAY_OPCODE_INTEGER_WRAPPING_NEGATE
        || instruction->opcode == MMAY_OPCODE_INTEGER_SATURATING_NEGATE;
    MarimayBytecodeType option_payload = MMAY_BYTECODE_TYPE_INVALID;
    if (target_type == NULL || first_type == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    if (assigned[first] == 0) {
        return MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
    }
    if (!mmay_bytecode_type_is_integer(*first_type)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_INTEGER_CONVERT_TOTAL) {
        if (!mmay_bytecode_type_is_integer(*target_type)
            || !mmay_bytecode_numeric_range_preserving(
                *first_type, *target_type
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    } else if (instruction->opcode == MMAY_OPCODE_INTEGER_CONVERT_OPTION) {
        if (*target_type != MMAY_BYTECODE_TYPE_ENUM
            || !mmay_bytecode_numeric_option_payload(
                function, target, &option_payload
            ) || mmay_bytecode_numeric_range_preserving(
                *first_type, option_payload
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    } else {
        if (!unary) {
            const MarimayBytecodeType *second_type =
                second < function->register_types.length
                ? mmay_vector_at_const(
                    &function->register_types, (size_t)second
                ) : NULL;
            if (second_type == NULL) {
                return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            if (assigned[second] == 0) {
                return MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
            }
            if (*second_type != *first_type) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        }
        if (checked) {
            if (*target_type != MMAY_BYTECODE_TYPE_ENUM
                || !mmay_bytecode_numeric_option_payload(
                    function, target, &option_payload
                ) || option_payload != *first_type) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else if (*target_type != *first_type) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    assigned[target] = 1;
    return MMAY_BYTECODE_VERIFY_OK;
}
