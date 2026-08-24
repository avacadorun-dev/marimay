#include "bytecode_internal.h"

static const MarimayBytecodeEnumVariant *mmay_bytecode_parse_variant(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant
) {
    return function != NULL && layout != NULL && variant < layout->variant_count
            && layout->variant_start <= function->enum_variants.length
            && variant < function->enum_variants.length - layout->variant_start
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + variant
        ) : NULL;
}

static const MarimayBytecodeEnumPayload *mmay_bytecode_parse_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant,
    size_t field
) {
    size_t relative;
    if (function == NULL || layout == NULL || variant == NULL
        || field >= variant->payload_count
        || !mmay_size_add(variant->payload_start, field, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_parse_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    uint64_t target_id;
    uint64_t text_id;
    uint64_t radix_id;
    const MarimayBytecodeType *target;
    const MarimayBytecodeType *text;
    const MarimayBytecodeType *radix;
    uint32_t result_id;
    const MarimayBytecodeEnumLayout *result_layout;
    const MarimayBytecodeEnumVariant *ok;
    const MarimayBytecodeEnumVariant *error;
    const MarimayBytecodeEnumPayload *ok_payload;
    const MarimayBytecodeEnumPayload *error_payload;
    const MarimayBytecodeEnumLayout *parse_error;
    const MarimayBytecodeEnumVariant *empty;
    const MarimayBytecodeEnumVariant *invalid_digit;
    const MarimayBytecodeEnumVariant *out_of_range;
    const MarimayBytecodeEnumVariant *invalid_radix;
    const MarimayBytecodeEnumPayload *offset;
    if (function == NULL || instruction == NULL || assigned == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    target_id = instruction->operands[0].unsigned_value;
    text_id = instruction->operands[1].unsigned_value;
    radix_id = instruction->operands[2].unsigned_value;
    if (target_id >= function->register_types.length
        || text_id >= function->register_types.length
        || radix_id >= function->register_types.length) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    if (assigned[text_id] == 0 || assigned[radix_id] == 0) {
        return MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
    }
    target = mmay_vector_at_const(&function->register_types, (size_t)target_id);
    text = mmay_vector_at_const(&function->register_types, (size_t)text_id);
    radix = mmay_vector_at_const(&function->register_types, (size_t)radix_id);
    result_id = mmay_bytecode_register_layout(function, (uint32_t)target_id);
    result_layout = mmay_bytecode_enum_layout(function, result_id);
    ok = mmay_bytecode_parse_variant(function, result_layout, 0);
    error = mmay_bytecode_parse_variant(function, result_layout, 1);
    ok_payload = mmay_bytecode_parse_payload(
        function, result_layout, ok, 0
    );
    error_payload = mmay_bytecode_parse_payload(
        function, result_layout, error, 0
    );
    parse_error = error_payload != NULL
            && error_payload->type == MMAY_BYTECODE_TYPE_ENUM
        ? mmay_bytecode_enum_layout(function, error_payload->layout) : NULL;
    empty = mmay_bytecode_parse_variant(function, parse_error, 0);
    invalid_digit = mmay_bytecode_parse_variant(function, parse_error, 1);
    out_of_range = mmay_bytecode_parse_variant(function, parse_error, 2);
    invalid_radix = mmay_bytecode_parse_variant(function, parse_error, 3);
    offset = mmay_bytecode_parse_payload(
        function, parse_error, invalid_digit, 0
    );
    return target != NULL && *target == MMAY_BYTECODE_TYPE_ENUM
            && text != NULL && *text == MMAY_BYTECODE_TYPE_STRING
            && radix != NULL && *radix == MMAY_BYTECODE_TYPE_I64
            && result_layout != NULL
            && result_layout->variant_count == 2
            && ok != NULL && ok->payload_count == 1
            && ok_payload != NULL
            && ok_payload->type == MMAY_BYTECODE_TYPE_I64
            && ok_payload->layout == MMAY_BYTECODE_NO_LAYOUT
            && error != NULL && error->payload_count == 1
            && error_payload != NULL && parse_error != NULL
            && parse_error->variant_count == 4
            && empty != NULL && empty->payload_count == 0
            && invalid_digit != NULL && invalid_digit->payload_count == 1
            && offset != NULL && offset->type == MMAY_BYTECODE_TYPE_I64
            && offset->layout == MMAY_BYTECODE_NO_LAYOUT
            && out_of_range != NULL && out_of_range->payload_count == 0
            && invalid_radix != NULL && invalid_radix->payload_count == 0
        ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
}
