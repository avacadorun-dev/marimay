#include "mir_internal.h"

#include "mmay/base/checked_integer.h"

static bool mmay_mir_numeric_kind(
    MarimayTypeId type,
    MarimayIntegerKind *kind
) {
    if (kind == NULL) {
        return false;
    }
    switch (type) {
        case MMAY_TYPE_I8: *kind = MMAY_INTEGER_I8; return true;
        case MMAY_TYPE_I16: *kind = MMAY_INTEGER_I16; return true;
        case MMAY_TYPE_I32: *kind = MMAY_INTEGER_I32; return true;
        case MMAY_TYPE_I64: *kind = MMAY_INTEGER_I64; return true;
        case MMAY_TYPE_U8: *kind = MMAY_INTEGER_U8; return true;
        case MMAY_TYPE_U16: *kind = MMAY_INTEGER_U16; return true;
        case MMAY_TYPE_U32: *kind = MMAY_INTEGER_U32; return true;
        case MMAY_TYPE_U64: *kind = MMAY_INTEGER_U64; return true;
        default: return false;
    }
}

static MarimayCheckedIntegerStatus mmay_mir_numeric_apply(
    const MarimayMirInstruction *instruction,
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *output
) {
    switch (instruction->integer_operation) {
        case MMAY_MIR_POLICY_ADD:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_add(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_add(left, right, output);
            return mmay_saturating_integer_add(left, right, output);
        case MMAY_MIR_POLICY_SUBTRACT:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_subtract(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_subtract(left, right, output);
            return mmay_saturating_integer_subtract(left, right, output);
        case MMAY_MIR_POLICY_MULTIPLY:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_multiply(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_multiply(left, right, output);
            return mmay_saturating_integer_multiply(left, right, output);
        case MMAY_MIR_POLICY_DIVIDE:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_divide(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_divide(left, right, output);
            return mmay_saturating_integer_divide(left, right, output);
        case MMAY_MIR_POLICY_REMAINDER:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_remainder(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_remainder(left, right, output);
            return mmay_saturating_integer_remainder(left, right, output);
        case MMAY_MIR_POLICY_SHIFT_LEFT:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_shift_left(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_shift_left(left, right, output);
            return mmay_saturating_integer_shift_left(left, right, output);
        case MMAY_MIR_POLICY_SHIFT_RIGHT:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION)
                return mmay_checked_integer_shift_right(left, right, output);
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING)
                return mmay_wrapping_integer_shift_right(left, right, output);
            return mmay_saturating_integer_shift_right(left, right, output);
        case MMAY_MIR_POLICY_NEGATE:
            if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION) {
                return mmay_checked_integer_negate(left, output);
            }
            if (instruction->integer_policy == MMAY_MIR_INTEGER_WRAPPING) {
                return mmay_wrapping_integer_negate(left, output);
            }
            return mmay_saturating_integer_negate(left, output);
    }
    return MMAY_CHECKED_INTEGER_OVERFLOW;
}

static MarimayMirInterpretStatus mmay_mir_numeric_status(
    MarimayCheckedIntegerStatus status
) {
    switch (status) {
        case MMAY_CHECKED_INTEGER_OK: return MMAY_MIR_INTERPRET_OK;
        case MMAY_CHECKED_INTEGER_OVERFLOW: return MMAY_MIR_INTERPRET_OVERFLOW;
        case MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO:
            return MMAY_MIR_INTERPRET_DIVISION_BY_ZERO;
        case MMAY_CHECKED_INTEGER_INVALID_SHIFT:
            return MMAY_MIR_INTERPRET_INVALID_SHIFT;
    }
    return MMAY_MIR_INTERPRET_INVALID_MIR;
}

static MarimayMirInterpretStatus mmay_mir_numeric_option(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirExecution *execution,
    bool some,
    MarimayMirValue payload,
    MarimayMirValue *target
) {
    const MarimayMirEnum *layout = mmay_mir_enum(
        function, instruction->enum_id
    );
    if (execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (layout == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    return mmay_mir_execution_new_enum(
        execution,
        function,
        layout,
        some ? 0 : 1,
        some ? &payload : NULL,
        some ? 1 : 0,
        &target->as.reference
    );
}

MarimayMirInterpretStatus mmay_mir_execute_numeric_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    MarimayIntegerKind source_kind;
    MarimayIntegerKind target_kind;
    MarimayIntegerValue left;
    MarimayIntegerValue right;
    MarimayIntegerValue output;
    MarimayCheckedIntegerStatus status;
    if (function == NULL || instruction == NULL || first == NULL
        || second == NULL || target == NULL
        || !mmay_mir_numeric_kind(first->type, &source_kind)) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    left = (MarimayIntegerValue){source_kind, first->as.integer_bits};
    if (instruction->kind == MMAY_MIR_INTEGER_CONVERT) {
        bool converted;
        MarimayMirValue payload;
        if (!mmay_mir_numeric_kind(
                instruction->integer_target_type, &target_kind
            )) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        converted = mmay_integer_convert(left, target_kind, &output);
        if (target->type == instruction->integer_target_type) {
            if (!converted) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            target->as.integer_bits = output.bits;
            return MMAY_MIR_INTERPRET_OK;
        }
        payload.type = instruction->integer_target_type;
        payload.as.integer_bits = converted ? output.bits : 0;
        return mmay_mir_numeric_option(
            function, instruction, options.execution,
            converted, payload, target
        );
    }
    right = (MarimayIntegerValue){source_kind, second->as.integer_bits};
    output = (MarimayIntegerValue){source_kind, 0};
    status = mmay_mir_numeric_apply(
        instruction, left, right, &output
    );
    if (instruction->integer_policy == MMAY_MIR_INTEGER_CHECKED_OPTION) {
        MarimayMirValue payload;
        payload.type = first->type;
        payload.as.integer_bits = status == MMAY_CHECKED_INTEGER_OK
            ? output.bits : 0;
        return mmay_mir_numeric_option(
            function, instruction, options.execution,
            status == MMAY_CHECKED_INTEGER_OK, payload, target
        );
    }
    if (status == MMAY_CHECKED_INTEGER_OK) {
        target->as.integer_bits = output.bits;
    }
    return mmay_mir_numeric_status(status);
}
