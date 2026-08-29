#include "bytecode_internal.h"

static const MarimayBytecodeEnumVariant *mmay_bytecode_context_variant(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t index
) {
    return layout != NULL && index < layout->variant_count
            && layout->variant_start <= function->enum_variants.length
            && index < function->enum_variants.length - layout->variant_start
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + index
        ) : NULL;
}

static const MarimayBytecodeEnumPayload *mmay_bytecode_context_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant,
    size_t index
) {
    size_t relative = 0;
    if (layout == NULL || variant == NULL || index >= variant->payload_count
        || !mmay_size_add(variant->payload_start, index, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static bool mmay_bytecode_context_payloads(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant_index,
    const MarimayBytecodeType *types,
    size_t count
) {
    const MarimayBytecodeEnumVariant *variant = mmay_bytecode_context_variant(
        function, layout, variant_index
    );
    size_t index = 0;
    if (variant == NULL || variant->payload_count != count) return false;
    for (index = 0; index < count; index += 1) {
        const MarimayBytecodeEnumPayload *payload =
            mmay_bytecode_context_payload(function, layout, variant, index);
        if (payload == NULL || payload->type != types[index]
            || payload->layout != MMAY_BYTECODE_NO_LAYOUT) return false;
    }
    return true;
}

static bool mmay_bytecode_context_error(
    const MarimayBytecodeFunction *function,
    uint32_t layout_id,
    bool environment
) {
    static const MarimayBytecodeType string[] = {MMAY_BYTECODE_TYPE_STRING};
    static const MarimayBytecodeType integer[] = {MMAY_BYTECODE_TYPE_I64};
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, layout_id
    );
    if (layout == NULL) return false;
    if (environment) {
        return layout->variant_count == 5
            && mmay_bytecode_context_payloads(function, layout, 0, NULL, 0)
            && mmay_bytecode_context_payloads(function, layout, 1, string, 1)
            && mmay_bytecode_context_payloads(function, layout, 2, integer, 1)
            && mmay_bytecode_context_payloads(function, layout, 3, integer, 1)
            && mmay_bytecode_context_payloads(function, layout, 4, NULL, 0);
    }
    return layout->variant_count == 4
        && mmay_bytecode_context_payloads(function, layout, 0, NULL, 0)
        && mmay_bytecode_context_payloads(function, layout, 1, NULL, 0)
        && mmay_bytecode_context_payloads(function, layout, 2, integer, 1)
        && mmay_bytecode_context_payloads(function, layout, 3, NULL, 0);
}

static bool mmay_bytecode_context_result(
    const MarimayBytecodeFunction *function,
    uint32_t layout_id,
    bool environment
) {
    static const MarimayBytecodeType string[] = {MMAY_BYTECODE_TYPE_STRING};
    const MarimayBytecodeEnumLayout *result = mmay_bytecode_enum_layout(
        function, layout_id
    );
    const MarimayBytecodeEnumVariant *ok = mmay_bytecode_context_variant(
        function, result, 0
    );
    const MarimayBytecodeEnumVariant *error = mmay_bytecode_context_variant(
        function, result, 1
    );
    const MarimayBytecodeEnumPayload *ok_payload = ok != NULL
            && ok->payload_count == 1
        ? mmay_bytecode_context_payload(function, result, ok, 0) : NULL;
    const MarimayBytecodeEnumPayload *error_payload = error != NULL
            && error->payload_count == 1
        ? mmay_bytecode_context_payload(function, result, error, 0) : NULL;
    if (result == NULL || result->variant_count != 2 || ok_payload == NULL
        || error_payload == NULL || error_payload->type != MMAY_BYTECODE_TYPE_ENUM
        || !mmay_bytecode_context_error(
            function, error_payload->layout, environment
        )) return false;
    if (!environment) {
        return ok_payload->type == MMAY_BYTECODE_TYPE_STRING
            && ok_payload->layout == MMAY_BYTECODE_NO_LAYOUT;
    }
    {
        const MarimayBytecodeEnumLayout *option = ok_payload->type
                == MMAY_BYTECODE_TYPE_ENUM
            ? mmay_bytecode_enum_layout(function, ok_payload->layout) : NULL;
        return option != NULL && option->variant_count == 2
            && mmay_bytecode_context_payloads(function, option, 0, string, 1)
            && mmay_bytecode_context_payloads(function, option, 1, NULL, 0);
    }
}

bool mmay_bytecode_opcode_is_context(MarimayOpcode opcode) {
    return opcode >= MMAY_OPCODE_ENV_GET
        && opcode <= MMAY_OPCODE_PROCESS_EXECUTABLE_PATH;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_context_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t source = instruction->operands[1].unsigned_value;
    const MarimayBytecodeType *target_type = target
            < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)target) : NULL;
    uint32_t layout = target_type != NULL
        ? mmay_bytecode_register_layout(function, (uint32_t)target)
        : MMAY_BYTECODE_NO_LAYOUT;
    bool environment = instruction->opcode == MMAY_OPCODE_ENV_GET;
    if (target_type == NULL) return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    if (environment) {
        const MarimayBytecodeType *source_type = source
                < function->register_types.length
            ? mmay_vector_at_const(
                &function->register_types, (size_t)source
            ) : NULL;
        if (source_type == NULL) return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
        if (assigned[source] == 0) {
            return MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
        }
        if (*source_type != MMAY_BYTECODE_TYPE_STRING) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    return *target_type == MMAY_BYTECODE_TYPE_ENUM
            && mmay_bytecode_context_result(function, layout, environment)
        ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
}
