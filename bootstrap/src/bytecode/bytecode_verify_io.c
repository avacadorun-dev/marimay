#include "bytecode_internal.h"

static const MarimayBytecodeEnumVariant *mmay_bytecode_io_variant(
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

static const MarimayBytecodeEnumPayload *mmay_bytecode_io_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant,
    size_t field
) {
    size_t relative = 0;
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

MarimayBytecodeVerifyStatus mmay_bytecode_verify_io_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t text = instruction->operands[1].unsigned_value;
    const MarimayBytecodeType *target_type = target < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)target) : NULL;
    const MarimayBytecodeType *text_type = text < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)text) : NULL;
    uint32_t result_id = target_type != NULL
        ? mmay_bytecode_register_layout(function, (uint32_t)target)
        : MMAY_BYTECODE_NO_LAYOUT;
    const MarimayBytecodeEnumLayout *result_layout = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumVariant *ok = mmay_bytecode_io_variant(
        function, result_layout, 0
    );
    const MarimayBytecodeEnumVariant *error = mmay_bytecode_io_variant(
        function, result_layout, 1
    );
    const MarimayBytecodeEnumPayload *error_payload = mmay_bytecode_io_payload(
        function, result_layout, error, 0
    );
    const MarimayBytecodeEnumLayout *io_error = error_payload != NULL
            && error_payload->type == MMAY_BYTECODE_TYPE_ENUM
        ? mmay_bytecode_enum_layout(function, error_payload->layout) : NULL;
    const MarimayBytecodeEnumVariant *capability = mmay_bytecode_io_variant(
        function, io_error, 0
    );
    const MarimayBytecodeEnumVariant *write_failed = mmay_bytecode_io_variant(
        function, io_error, 1
    );
    const MarimayBytecodeEnumPayload *capability_payload =
        mmay_bytecode_io_payload(function, io_error, capability, 0);
    if (target_type == NULL || text_type == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return *target_type == MMAY_BYTECODE_TYPE_ENUM
            && *text_type == MMAY_BYTECODE_TYPE_STRING
            && result_layout != NULL && result_layout->variant_count == 2
            && ok != NULL && ok->payload_count == 0
            && error != NULL && error->payload_count == 1
            && error_payload != NULL
            && io_error != NULL && io_error->variant_count == 2
            && capability != NULL && capability->payload_count == 1
            && capability_payload != NULL
            && capability_payload->type == MMAY_BYTECODE_TYPE_STRING
            && capability_payload->layout == MMAY_BYTECODE_NO_LAYOUT
            && write_failed != NULL && write_failed->payload_count == 0
        ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
}
