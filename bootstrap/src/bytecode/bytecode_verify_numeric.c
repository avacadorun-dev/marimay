#include "bytecode_internal.h"

#include <string.h>

bool ruda_bytecode_opcode_is_numeric(RudaOpcode opcode) {
    return (opcode >= RUDA_OPCODE_INTEGER_CHECKED_ADD
            && opcode <= RUDA_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT)
        || opcode == RUDA_OPCODE_INTEGER_CONVERT_TOTAL
        || opcode == RUDA_OPCODE_INTEGER_CONVERT_OPTION;
}

bool ruda_bytecode_numeric_option_symbol(
    RudaBytecodeType payload_type,
    RudaSymbolId *symbol
) {
    if (symbol == NULL || !ruda_bytecode_type_is_integer(payload_type)) {
        return false;
    }
    return ruda_symbol_id_from_bytes(
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

static bool ruda_bytecode_numeric_signed(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_I8
        || type == RUDA_BYTECODE_TYPE_I16
        || type == RUDA_BYTECODE_TYPE_I32
        || type == RUDA_BYTECODE_TYPE_I64;
}

static unsigned int ruda_bytecode_numeric_width(RudaBytecodeType type) {
    switch (type) {
        case RUDA_BYTECODE_TYPE_I8: case RUDA_BYTECODE_TYPE_U8: return 8;
        case RUDA_BYTECODE_TYPE_I16: case RUDA_BYTECODE_TYPE_U16: return 16;
        case RUDA_BYTECODE_TYPE_I32: case RUDA_BYTECODE_TYPE_U32: return 32;
        case RUDA_BYTECODE_TYPE_I64: case RUDA_BYTECODE_TYPE_U64: return 64;
        default: return 0;
    }
}

static bool ruda_bytecode_numeric_range_preserving(
    RudaBytecodeType source,
    RudaBytecodeType target
) {
    unsigned int source_width = ruda_bytecode_numeric_width(source);
    unsigned int target_width = ruda_bytecode_numeric_width(target);
    bool source_signed = ruda_bytecode_numeric_signed(source);
    bool target_signed = ruda_bytecode_numeric_signed(target);
    if (source_width == 0 || target_width == 0) {
        return false;
    }
    if (source_signed == target_signed) {
        return source_width <= target_width;
    }
    return !source_signed && target_signed && source_width < target_width;
}

static bool ruda_bytecode_numeric_option_payload(
    const RudaBytecodeFunction *function,
    uint64_t target,
    RudaBytecodeType *payload_type
) {
    RudaSymbolId option_symbol;
    uint32_t layout_id = target <= UINT32_MAX
        ? ruda_bytecode_register_layout(function, (uint32_t)target)
        : RUDA_BYTECODE_NO_LAYOUT;
    const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
        function, layout_id
    );
    const RudaBytecodeEnumVariant *some = layout != NULL
            && layout->variant_count == 2
        ? ruda_vector_at_const(
            &function->enum_variants, layout->variant_start
        ) : NULL;
    const RudaBytecodeEnumVariant *none = layout != NULL
            && layout->variant_count == 2
        ? ruda_vector_at_const(
            &function->enum_variants, layout->variant_start + 1
        ) : NULL;
    const RudaBytecodeEnumPayload *payload = some != NULL
            && some->payload_count == 1
        ? ruda_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + some->payload_start
        ) : NULL;
    if (payload_type == NULL || layout == NULL
        || layout->payload_count != 1 || some == NULL
        || some->payload_start != 0 || some->payload_count != 1
        || none == NULL || none->payload_count != 0 || payload == NULL
        || payload->layout != RUDA_BYTECODE_NO_LAYOUT
        || !ruda_bytecode_type_is_integer(payload->type)
        || !ruda_bytecode_numeric_option_symbol(
            payload->type, &option_symbol
        ) || memcmp(layout->symbol.bytes, option_symbol.bytes, 16) != 0) {
        return false;
    }
    *payload_type = payload->type;
    return true;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_numeric_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = instruction->operands[1].unsigned_value;
    uint64_t second = instruction->operands[2].unsigned_value;
    const RudaBytecodeType *target_type = target < function->register_types.length
        ? ruda_vector_at_const(&function->register_types, (size_t)target)
        : NULL;
    const RudaBytecodeType *first_type = first < function->register_types.length
        ? ruda_vector_at_const(&function->register_types, (size_t)first)
        : NULL;
    bool checked = instruction->opcode >= RUDA_OPCODE_INTEGER_CHECKED_ADD
        && instruction->opcode <= RUDA_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT;
    bool unary = instruction->opcode == RUDA_OPCODE_INTEGER_CHECKED_NEGATE
        || instruction->opcode == RUDA_OPCODE_INTEGER_WRAPPING_NEGATE
        || instruction->opcode == RUDA_OPCODE_INTEGER_SATURATING_NEGATE;
    RudaBytecodeType option_payload = RUDA_BYTECODE_TYPE_INVALID;
    if (target_type == NULL || first_type == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    if (assigned[first] == 0) {
        return RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
    }
    if (!ruda_bytecode_type_is_integer(*first_type)) {
        return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_INTEGER_CONVERT_TOTAL) {
        if (!ruda_bytecode_type_is_integer(*target_type)
            || !ruda_bytecode_numeric_range_preserving(
                *first_type, *target_type
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    } else if (instruction->opcode == RUDA_OPCODE_INTEGER_CONVERT_OPTION) {
        if (*target_type != RUDA_BYTECODE_TYPE_ENUM
            || !ruda_bytecode_numeric_option_payload(
                function, target, &option_payload
            ) || ruda_bytecode_numeric_range_preserving(
                *first_type, option_payload
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    } else {
        if (!unary) {
            const RudaBytecodeType *second_type =
                second < function->register_types.length
                ? ruda_vector_at_const(
                    &function->register_types, (size_t)second
                ) : NULL;
            if (second_type == NULL) {
                return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            if (assigned[second] == 0) {
                return RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
            }
            if (*second_type != *first_type) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        }
        if (checked) {
            if (*target_type != RUDA_BYTECODE_TYPE_ENUM
                || !ruda_bytecode_numeric_option_payload(
                    function, target, &option_payload
                ) || option_payload != *first_type) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
        } else if (*target_type != *first_type) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    assigned[target] = 1;
    return RUDA_BYTECODE_VERIFY_OK;
}
