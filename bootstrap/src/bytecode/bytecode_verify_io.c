#include "bytecode_internal.h"

static const RudaBytecodeEnumVariant *ruda_bytecode_io_variant(
    const RudaBytecodeFunction *function,
    const RudaBytecodeEnumLayout *layout,
    size_t variant
) {
    return function != NULL && layout != NULL && variant < layout->variant_count
        && layout->variant_start <= function->enum_variants.length
        && variant < function->enum_variants.length - layout->variant_start
        ? ruda_vector_at_const(
            &function->enum_variants, layout->variant_start + variant
        ) : NULL;
}

static const RudaBytecodeEnumPayload *ruda_bytecode_io_payload(
    const RudaBytecodeFunction *function,
    const RudaBytecodeEnumLayout *layout,
    const RudaBytecodeEnumVariant *variant,
    size_t field
) {
    size_t relative = 0;
    if (function == NULL || layout == NULL || variant == NULL
        || field >= variant->payload_count
        || !ruda_size_add(variant->payload_start, field, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return ruda_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_io_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t text = instruction->operands[1].unsigned_value;
    const RudaBytecodeType *target_type = target < function->register_types.length
        ? ruda_vector_at_const(&function->register_types, (size_t)target) : NULL;
    const RudaBytecodeType *text_type = text < function->register_types.length
        ? ruda_vector_at_const(&function->register_types, (size_t)text) : NULL;
    uint32_t result_id = target_type != NULL
        ? ruda_bytecode_register_layout(function, (uint32_t)target)
        : RUDA_BYTECODE_NO_LAYOUT;
    const RudaBytecodeEnumLayout *result_layout = ruda_bytecode_enum_layout(
        function, result_id
    );
    const RudaBytecodeEnumVariant *ok = ruda_bytecode_io_variant(
        function, result_layout, 0
    );
    const RudaBytecodeEnumVariant *error = ruda_bytecode_io_variant(
        function, result_layout, 1
    );
    const RudaBytecodeEnumPayload *error_payload = ruda_bytecode_io_payload(
        function, result_layout, error, 0
    );
    const RudaBytecodeEnumLayout *io_error = error_payload != NULL
            && error_payload->type == RUDA_BYTECODE_TYPE_ENUM
        ? ruda_bytecode_enum_layout(function, error_payload->layout) : NULL;
    const RudaBytecodeEnumVariant *capability = ruda_bytecode_io_variant(
        function, io_error, 0
    );
    const RudaBytecodeEnumVariant *write_failed = ruda_bytecode_io_variant(
        function, io_error, 1
    );
    const RudaBytecodeEnumPayload *capability_payload =
        ruda_bytecode_io_payload(function, io_error, capability, 0);
    if (target_type == NULL || text_type == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return *target_type == RUDA_BYTECODE_TYPE_ENUM
            && *text_type == RUDA_BYTECODE_TYPE_STRING
            && result_layout != NULL && result_layout->variant_count == 2
            && ok != NULL && ok->payload_count == 0
            && error != NULL && error->payload_count == 1
            && error_payload != NULL
            && io_error != NULL && io_error->variant_count == 2
            && capability != NULL && capability->payload_count == 1
            && capability_payload != NULL
            && capability_payload->type == RUDA_BYTECODE_TYPE_STRING
            && capability_payload->layout == RUDA_BYTECODE_NO_LAYOUT
            && write_failed != NULL && write_failed->payload_count == 0
        ? RUDA_BYTECODE_VERIFY_OK : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
}
