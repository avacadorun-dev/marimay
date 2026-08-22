#include "c_emitter_internal.h"

static MarimayTypeId mmay_native_c_numeric_register_type(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, register_id) : NULL;
    return type != NULL ? *type : MMAY_TYPE_INVALID;
}

static const char *mmay_native_c_numeric_operation(
    MarimayMirIntegerPolicy policy,
    MarimayMirIntegerOperation operation
) {
    static const char *const names[3][8] = {
        {
            "mmay_checked_integer_add",
            "mmay_checked_integer_subtract",
            "mmay_checked_integer_multiply",
            "mmay_checked_integer_divide",
            "mmay_checked_integer_remainder",
            "mmay_checked_integer_negate",
            "mmay_checked_integer_shift_left",
            "mmay_checked_integer_shift_right"
        },
        {
            "mmay_wrapping_integer_add",
            "mmay_wrapping_integer_subtract",
            "mmay_wrapping_integer_multiply",
            "mmay_wrapping_integer_divide",
            "mmay_wrapping_integer_remainder",
            "mmay_wrapping_integer_negate",
            "mmay_wrapping_integer_shift_left",
            "mmay_wrapping_integer_shift_right"
        },
        {
            "mmay_saturating_integer_add",
            "mmay_saturating_integer_subtract",
            "mmay_saturating_integer_multiply",
            "mmay_saturating_integer_divide",
            "mmay_saturating_integer_remainder",
            "mmay_saturating_integer_negate",
            "mmay_saturating_integer_shift_left",
            "mmay_saturating_integer_shift_right"
        }
    };
    if (policy > MMAY_MIR_INTEGER_SATURATING
        || operation > MMAY_MIR_POLICY_SHIFT_RIGHT) {
        return NULL;
    }
    return names[policy][operation];
}

static const char *mmay_native_c_numeric_constructor(MarimayTypeId type) {
    switch (type) {
        case MMAY_TYPE_I8: return "mmay_integer_from_i8";
        case MMAY_TYPE_I16: return "mmay_integer_from_i16";
        case MMAY_TYPE_I32: return "mmay_integer_from_i32";
        case MMAY_TYPE_I64: return "mmay_integer_from_i64";
        case MMAY_TYPE_U8: return "mmay_integer_from_u8";
        case MMAY_TYPE_U16: return "mmay_integer_from_u16";
        case MMAY_TYPE_U32: return "mmay_integer_from_u32";
        case MMAY_TYPE_U64: return "mmay_integer_from_u64";
        default: return NULL;
    }
}

static bool mmay_native_c_emit_numeric_option(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const char *success_expression
) {
    const MarimayMirEnum *enumeration = mmay_vector_at_const(
        &function->enums, instruction->enum_id
    );
    if (enumeration == NULL || success_expression == NULL
        || !mmay_native_c_write(
            buffer,
            " MarimayManagedCell mmay_g_cell; mmay_g_cell.f64_bits = mmay_g_out_value.bits; mmay_g_m = mmay_managed_enum_new(&mmay_g_x->heap, &"
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, enumeration->type
        ) || !mmay_native_c_write(
            buffer,
            ", %s ? 0 : 1, %s ? &mmay_g_cell : NULL, %s ? 1 : 0, &mmay_g_v_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }",
            success_expression,
            success_expression,
            success_expression,
            instruction->target
        )) {
        return false;
    }
    return true;
}

static bool mmay_native_c_emit_integer_policy(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId type = mmay_native_c_numeric_register_type(
        function, instruction->first
    );
    const char *kind = mmay_native_c_integer_kind_name(type);
    const char *constructor = mmay_native_c_numeric_constructor(type);
    const char *convert = mmay_native_c_integer_conversion_name(type);
    const char *operation = mmay_native_c_numeric_operation(
        instruction->integer_policy, instruction->integer_operation
    );
    bool unary = instruction->integer_operation == MMAY_MIR_POLICY_NEGATE;
    if (kind == NULL || constructor == NULL || convert == NULL
        || operation == NULL
        || !mmay_native_c_write(
            buffer,
            "  { MarimayIntegerValue mmay_g_left = %s(mmay_g_v_%u); MarimayIntegerValue mmay_g_out_value = {%s, 0}; MarimayCheckedIntegerStatus mmay_g_numeric_status; ",
            constructor, instruction->first, kind
        )) {
        return false;
    }
    if (unary) {
        if (!mmay_native_c_write(
                buffer,
                "mmay_g_numeric_status = %s(mmay_g_left, &mmay_g_out_value);",
                operation
            )) {
            return false;
        }
    } else if (!mmay_native_c_write(
            buffer,
            "MarimayIntegerValue mmay_g_right = %s(mmay_g_v_%u); mmay_g_numeric_status = %s(mmay_g_left, mmay_g_right, &mmay_g_out_value);",
            constructor, instruction->second, operation
        )) {
        return false;
    }
    if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION) {
        if (!mmay_native_c_emit_numeric_option(
                buffer, function, instruction,
                "mmay_g_numeric_status == MMAY_CHECKED_INTEGER_OK"
            )) {
            return false;
        }
    } else if (!mmay_native_c_write(
            buffer,
            " if (mmay_g_numeric_status != MMAY_CHECKED_INTEGER_OK || !%s(mmay_g_out_value, &mmay_g_v_%u)) { mmay_g_s = (int)(mmay_g_numeric_status != MMAY_CHECKED_INTEGER_OK ? mmay_g_numeric_status : MMAY_CHECKED_INTEGER_OVERFLOW); *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }",
            convert,
            instruction->target,
            instruction->span.start,
            instruction->span.end
        )) {
        return false;
    }
    return mmay_native_c_write(buffer, " }\n");
}

static bool mmay_native_c_emit_integer_conversion(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId source_type = mmay_native_c_numeric_register_type(
        function, instruction->first
    );
    MarimayTypeId target_type = instruction->integer_target_type;
    MarimayTypeId register_type = mmay_native_c_numeric_register_type(
        function, instruction->target
    );
    const char *source_constructor = mmay_native_c_numeric_constructor(
        source_type
    );
    const char *target_kind = mmay_native_c_integer_kind_name(target_type);
    const char *convert = mmay_native_c_integer_conversion_name(target_type);
    bool option = register_type != target_type;
    if (source_constructor == NULL || target_kind == NULL || convert == NULL
        || !mmay_native_c_write(
            buffer,
            "  { MarimayIntegerValue mmay_g_left = %s(mmay_g_v_%u); MarimayIntegerValue mmay_g_out_value = {%s, 0}; bool mmay_g_converted = mmay_integer_convert(mmay_g_left, %s, &mmay_g_out_value);",
            source_constructor, instruction->first, target_kind, target_kind
        )) {
        return false;
    }
    if (option) {
        if (!mmay_native_c_emit_numeric_option(
                buffer, function, instruction, "mmay_g_converted"
            )) {
            return false;
        }
    } else if (!mmay_native_c_write(
            buffer,
            " if (!mmay_g_converted || !%s(mmay_g_out_value, &mmay_g_v_%u)) { mmay_g_s = 7; *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }",
            convert,
            instruction->target,
            instruction->span.start,
            instruction->span.end
        )) {
        return false;
    }
    return mmay_native_c_write(buffer, " }\n");
}

bool mmay_native_c_emit_numeric_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    if (instruction == NULL) {
        return false;
    }
    if (instruction->kind == MMAY_MIR_INTEGER_POLICY) {
        return mmay_native_c_emit_integer_policy(
            buffer, function, instruction
        );
    }
    return instruction->kind == MMAY_MIR_INTEGER_CONVERT
        && mmay_native_c_emit_integer_conversion(
            buffer, function, instruction
        );
}
