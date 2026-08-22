#include "mir_internal.h"

static bool mmay_mir_numeric_integer_type(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64;
}

static unsigned int mmay_mir_numeric_width(MarimayTypeId type) {
    switch (type) {
        case MMAY_TYPE_I8: case MMAY_TYPE_U8: return 8;
        case MMAY_TYPE_I16: case MMAY_TYPE_U16: return 16;
        case MMAY_TYPE_I32: case MMAY_TYPE_U32: return 32;
        case MMAY_TYPE_I64: case MMAY_TYPE_U64: return 64;
        default: return 0;
    }
}

static bool mmay_mir_numeric_signed(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_I64;
}

static bool mmay_mir_numeric_range_preserving(
    MarimayTypeId source,
    MarimayTypeId target
) {
    unsigned int source_width = mmay_mir_numeric_width(source);
    unsigned int target_width = mmay_mir_numeric_width(target);
    bool source_signed = mmay_mir_numeric_signed(source);
    bool target_signed = mmay_mir_numeric_signed(target);
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

static bool mmay_mir_numeric_option_layout(
    const MarimayMirFunction *function,
    MarimayMirEnumId enum_id,
    MarimayTypeId option_type,
    MarimayTypeId payload_type
) {
    const MarimayMirEnum *layout = mmay_mir_enum(function, enum_id);
    const MarimayMirEnumVariant *some = mmay_mir_enum_variant(
        function, layout, 0
    );
    const MarimayMirEnumVariant *none = mmay_mir_enum_variant(
        function, layout, 1
    );
    const MarimayMirEnumPayload *payload = layout != NULL && some != NULL
            && some->payload_count == 1
        ? mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + some->payload_start
        )
        : NULL;
    return layout != NULL && layout->type == option_type
        && layout->variant_count == 2 && layout->payload_count == 1
        && some != NULL && some->kind == MMAY_MIR_ENUM_VARIANT_POSITIONAL
        && some->payload_start == 0 && some->payload_count == 1
        && none != NULL && none->kind == MMAY_MIR_ENUM_VARIANT_UNIT
        && none->payload_count == 0 && payload != NULL
        && payload->type == payload_type;
}

MarimayMirVerifyStatus mmay_mir_verify_numeric_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId target = mmay_mir_register_type(
        function, instruction->target
    );
    MarimayTypeId first = mmay_mir_register_type(
        function, instruction->first
    );
    MarimayTypeId second = mmay_mir_register_type(
        function, instruction->second
    );
    if (target == MMAY_TYPE_INVALID || first == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (!mmay_mir_numeric_integer_type(first)) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind == MMAY_MIR_INTEGER_CONVERT) {
        bool total = mmay_mir_numeric_range_preserving(
            first, instruction->integer_target_type
        );
        if (!mmay_mir_numeric_integer_type(instruction->integer_target_type)) {
            return MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        if (total) {
            return target == instruction->integer_target_type
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        return mmay_mir_numeric_option_layout(
                function, instruction->enum_id, target,
                instruction->integer_target_type
            )
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind != MMAY_MIR_INTEGER_POLICY
        || instruction->integer_policy > MMAY_MIR_INTEGER_SATURATING
        || instruction->integer_operation > MMAY_MIR_POLICY_SHIFT_RIGHT) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (second == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    } else if (second != first) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION) {
        return mmay_mir_numeric_option_layout(
                function, instruction->enum_id, target, first
            )
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    return target == first
        ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
}
