#include "c_emitter_internal.h"

static RudaTypeId ruda_native_c_numeric_register_type(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    const RudaTypeId *type = function != NULL
        ? ruda_vector_at_const(&function->register_types, register_id) : NULL;
    return type != NULL ? *type : RUDA_TYPE_INVALID;
}

static const char *ruda_native_c_numeric_operation(
    RudaMirIntegerPolicy policy,
    RudaMirIntegerOperation operation
) {
    static const char *const names[3][8] = {
        {
            "ruda_checked_integer_add",
            "ruda_checked_integer_subtract",
            "ruda_checked_integer_multiply",
            "ruda_checked_integer_divide",
            "ruda_checked_integer_remainder",
            "ruda_checked_integer_negate",
            "ruda_checked_integer_shift_left",
            "ruda_checked_integer_shift_right"
        },
        {
            "ruda_wrapping_integer_add",
            "ruda_wrapping_integer_subtract",
            "ruda_wrapping_integer_multiply",
            "ruda_wrapping_integer_divide",
            "ruda_wrapping_integer_remainder",
            "ruda_wrapping_integer_negate",
            "ruda_wrapping_integer_shift_left",
            "ruda_wrapping_integer_shift_right"
        },
        {
            "ruda_saturating_integer_add",
            "ruda_saturating_integer_subtract",
            "ruda_saturating_integer_multiply",
            "ruda_saturating_integer_divide",
            "ruda_saturating_integer_remainder",
            "ruda_saturating_integer_negate",
            "ruda_saturating_integer_shift_left",
            "ruda_saturating_integer_shift_right"
        }
    };
    if (policy > RUDA_MIR_INTEGER_SATURATING
        || operation > RUDA_MIR_POLICY_SHIFT_RIGHT) {
        return NULL;
    }
    return names[policy][operation];
}

static const char *ruda_native_c_numeric_constructor(RudaTypeId type) {
    switch (type) {
        case RUDA_TYPE_I8: return "ruda_integer_from_i8";
        case RUDA_TYPE_I16: return "ruda_integer_from_i16";
        case RUDA_TYPE_I32: return "ruda_integer_from_i32";
        case RUDA_TYPE_I64: return "ruda_integer_from_i64";
        case RUDA_TYPE_U8: return "ruda_integer_from_u8";
        case RUDA_TYPE_U16: return "ruda_integer_from_u16";
        case RUDA_TYPE_U32: return "ruda_integer_from_u32";
        case RUDA_TYPE_U64: return "ruda_integer_from_u64";
        default: return NULL;
    }
}

static bool ruda_native_c_emit_numeric_option(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const char *success_expression
) {
    const RudaMirEnum *enumeration = ruda_vector_at_const(
        &function->enums, instruction->enum_id
    );
    if (enumeration == NULL || success_expression == NULL
        || !ruda_native_c_write(
            buffer,
            " RudaManagedCell ruda_g_cell; ruda_g_cell.f64_bits = ruda_g_out_value.bits; ruda_g_m = ruda_managed_enum_new(&ruda_g_x->heap, &"
        ) || !ruda_native_c_write_descriptor_name(
            buffer, RUDA_NATIVE_C_MANAGED_ENUM, enumeration->type
        ) || !ruda_native_c_write(
            buffer,
            ", %s ? 0 : 1, %s ? &ruda_g_cell : NULL, %s ? 1 : 0, &ruda_g_v_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; }",
            success_expression,
            success_expression,
            success_expression,
            instruction->target
        )) {
        return false;
    }
    return true;
}

static bool ruda_native_c_emit_integer_policy(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    RudaTypeId type = ruda_native_c_numeric_register_type(
        function, instruction->first
    );
    const char *kind = ruda_native_c_integer_kind_name(type);
    const char *constructor = ruda_native_c_numeric_constructor(type);
    const char *convert = ruda_native_c_integer_conversion_name(type);
    const char *operation = ruda_native_c_numeric_operation(
        instruction->integer_policy, instruction->integer_operation
    );
    bool unary = instruction->integer_operation == RUDA_MIR_POLICY_NEGATE;
    if (kind == NULL || constructor == NULL || convert == NULL
        || operation == NULL
        || !ruda_native_c_write(
            buffer,
            "  { RudaIntegerValue ruda_g_left = %s(ruda_g_v_%u); RudaIntegerValue ruda_g_out_value = {%s, 0}; RudaCheckedIntegerStatus ruda_g_numeric_status; ",
            constructor, instruction->first, kind
        )) {
        return false;
    }
    if (unary) {
        if (!ruda_native_c_write(
                buffer,
                "ruda_g_numeric_status = %s(ruda_g_left, &ruda_g_out_value);",
                operation
            )) {
            return false;
        }
    } else if (!ruda_native_c_write(
            buffer,
            "RudaIntegerValue ruda_g_right = %s(ruda_g_v_%u); ruda_g_numeric_status = %s(ruda_g_left, ruda_g_right, &ruda_g_out_value);",
            constructor, instruction->second, operation
        )) {
        return false;
    }
    if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION) {
        if (!ruda_native_c_emit_numeric_option(
                buffer, function, instruction,
                "ruda_g_numeric_status == RUDA_CHECKED_INTEGER_OK"
            )) {
            return false;
        }
    } else if (!ruda_native_c_write(
            buffer,
            " if (ruda_g_numeric_status != RUDA_CHECKED_INTEGER_OK || !%s(ruda_g_out_value, &ruda_g_v_%u)) { ruda_g_s = (int)(ruda_g_numeric_status != RUDA_CHECKED_INTEGER_OK ? ruda_g_numeric_status : RUDA_CHECKED_INTEGER_OVERFLOW); *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; }",
            convert,
            instruction->target,
            instruction->span.start,
            instruction->span.end
        )) {
        return false;
    }
    return ruda_native_c_write(buffer, " }\n");
}

static bool ruda_native_c_emit_integer_conversion(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    RudaTypeId source_type = ruda_native_c_numeric_register_type(
        function, instruction->first
    );
    RudaTypeId target_type = instruction->integer_target_type;
    RudaTypeId register_type = ruda_native_c_numeric_register_type(
        function, instruction->target
    );
    const char *source_constructor = ruda_native_c_numeric_constructor(
        source_type
    );
    const char *target_kind = ruda_native_c_integer_kind_name(target_type);
    const char *convert = ruda_native_c_integer_conversion_name(target_type);
    bool option = register_type != target_type;
    if (source_constructor == NULL || target_kind == NULL || convert == NULL
        || !ruda_native_c_write(
            buffer,
            "  { RudaIntegerValue ruda_g_left = %s(ruda_g_v_%u); RudaIntegerValue ruda_g_out_value = {%s, 0}; bool ruda_g_converted = ruda_integer_convert(ruda_g_left, %s, &ruda_g_out_value);",
            source_constructor, instruction->first, target_kind, target_kind
        )) {
        return false;
    }
    if (option) {
        if (!ruda_native_c_emit_numeric_option(
                buffer, function, instruction, "ruda_g_converted"
            )) {
            return false;
        }
    } else if (!ruda_native_c_write(
            buffer,
            " if (!ruda_g_converted || !%s(ruda_g_out_value, &ruda_g_v_%u)) { ruda_g_s = 7; *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; }",
            convert,
            instruction->target,
            instruction->span.start,
            instruction->span.end
        )) {
        return false;
    }
    return ruda_native_c_write(buffer, " }\n");
}

bool ruda_native_c_emit_numeric_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    if (instruction == NULL) {
        return false;
    }
    if (instruction->kind == RUDA_MIR_INTEGER_POLICY) {
        return ruda_native_c_emit_integer_policy(
            buffer, function, instruction
        );
    }
    return instruction->kind == RUDA_MIR_INTEGER_CONVERT
        && ruda_native_c_emit_integer_conversion(
            buffer, function, instruction
        );
}
