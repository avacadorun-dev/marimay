#include "mir_internal.h"

static bool ruda_mir_numeric_integer_type(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_U64;
}

static unsigned int ruda_mir_numeric_width(RudaTypeId type) {
    switch (type) {
        case RUDA_TYPE_I8: case RUDA_TYPE_U8: return 8;
        case RUDA_TYPE_I16: case RUDA_TYPE_U16: return 16;
        case RUDA_TYPE_I32: case RUDA_TYPE_U32: return 32;
        case RUDA_TYPE_I64: case RUDA_TYPE_U64: return 64;
        default: return 0;
    }
}

static bool ruda_mir_numeric_signed(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_I64;
}

static bool ruda_mir_numeric_range_preserving(
    RudaTypeId source,
    RudaTypeId target
) {
    unsigned int source_width = ruda_mir_numeric_width(source);
    unsigned int target_width = ruda_mir_numeric_width(target);
    bool source_signed = ruda_mir_numeric_signed(source);
    bool target_signed = ruda_mir_numeric_signed(target);
    if (source_width == 0 || target_width == 0) {
        return false;
    }
    if (source_signed == target_signed) {
        return source_width <= target_width;
    }
    if (source_signed) {
        return false;
    }
    return source_width < target_width;
}

static bool ruda_mir_numeric_option_layout(
    const RudaMirFunction *function,
    RudaMirEnumId enum_id,
    RudaTypeId option_type,
    RudaTypeId payload_type
) {
    const RudaMirEnum *layout = ruda_mir_enum(function, enum_id);
    const RudaMirEnumVariant *some = ruda_mir_enum_variant(
        function, layout, 0
    );
    const RudaMirEnumVariant *none = ruda_mir_enum_variant(
        function, layout, 1
    );
    const RudaMirEnumPayload *payload = layout != NULL && some != NULL
            && some->payload_count == 1
        ? ruda_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + some->payload_start
        )
        : NULL;
    return layout != NULL && layout->type == option_type
        && layout->variant_count == 2 && layout->payload_count == 1
        && some != NULL && some->kind == RUDA_MIR_ENUM_VARIANT_POSITIONAL
        && some->payload_start == 0 && some->payload_count == 1
        && none != NULL && none->kind == RUDA_MIR_ENUM_VARIANT_UNIT
        && none->payload_count == 0 && payload != NULL
        && payload->type == payload_type;
}

RudaMirVerifyStatus ruda_mir_verify_numeric_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    RudaTypeId target = ruda_mir_register_type(
        function, instruction->target
    );
    RudaTypeId first = ruda_mir_register_type(
        function, instruction->first
    );
    RudaTypeId second = ruda_mir_register_type(
        function, instruction->second
    );
    if (target == RUDA_TYPE_INVALID || first == RUDA_TYPE_INVALID) {
        return RUDA_MIR_VERIFY_INVALID_REGISTER;
    }
    if (!ruda_mir_numeric_integer_type(first)) {
        return RUDA_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == RUDA_MIR_INTEGER_CONVERT) {
        bool total = ruda_mir_numeric_range_preserving(
            first, instruction->integer_target_type
        );
        if (!ruda_mir_numeric_integer_type(instruction->integer_target_type)) {
            return RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        if (total) {
            return target == instruction->integer_target_type
                ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        return ruda_mir_numeric_option_layout(
                function, instruction->enum_id, target,
                instruction->integer_target_type
            )
            ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind != RUDA_MIR_INTEGER_POLICY
        || instruction->integer_policy > RUDA_MIR_INTEGER_SATURATING
        || instruction->integer_operation > RUDA_MIR_POLICY_SHIFT_RIGHT) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (second == RUDA_TYPE_INVALID) {
        return RUDA_MIR_VERIFY_INVALID_REGISTER;
    } else if (second != first) {
        return RUDA_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION) {
        return ruda_mir_numeric_option_layout(
                function, instruction->enum_id, target, first
            )
            ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
    }
    return target == first
        ? RUDA_MIR_VERIFY_OK : RUDA_MIR_VERIFY_TYPE_MISMATCH;
}
