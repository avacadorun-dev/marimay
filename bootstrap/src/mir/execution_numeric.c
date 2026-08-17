#include "mir_internal.h"

#include "ruda/base/checked_integer.h"

static bool ruda_mir_numeric_kind(
    RudaTypeId type,
    RudaIntegerKind *kind
) {
    if (kind == NULL) {
        return false;
    }
    switch (type) {
        case RUDA_TYPE_I8: *kind = RUDA_INTEGER_I8; return true;
        case RUDA_TYPE_I16: *kind = RUDA_INTEGER_I16; return true;
        case RUDA_TYPE_I32: *kind = RUDA_INTEGER_I32; return true;
        case RUDA_TYPE_I64: *kind = RUDA_INTEGER_I64; return true;
        case RUDA_TYPE_U8: *kind = RUDA_INTEGER_U8; return true;
        case RUDA_TYPE_U16: *kind = RUDA_INTEGER_U16; return true;
        case RUDA_TYPE_U32: *kind = RUDA_INTEGER_U32; return true;
        case RUDA_TYPE_U64: *kind = RUDA_INTEGER_U64; return true;
        default: return false;
    }
}

static RudaCheckedIntegerStatus ruda_mir_numeric_apply(
    const RudaMirInstruction *instruction,
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *output
) {
    switch (instruction->integer_operation) {
        case RUDA_MIR_POLICY_ADD:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_add(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_add(left, right, output);
            return ruda_saturating_integer_add(left, right, output);
        case RUDA_MIR_POLICY_SUBTRACT:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_subtract(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_subtract(left, right, output);
            return ruda_saturating_integer_subtract(left, right, output);
        case RUDA_MIR_POLICY_MULTIPLY:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_multiply(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_multiply(left, right, output);
            return ruda_saturating_integer_multiply(left, right, output);
        case RUDA_MIR_POLICY_DIVIDE:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_divide(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_divide(left, right, output);
            return ruda_saturating_integer_divide(left, right, output);
        case RUDA_MIR_POLICY_REMAINDER:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_remainder(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_remainder(left, right, output);
            return ruda_saturating_integer_remainder(left, right, output);
        case RUDA_MIR_POLICY_SHIFT_LEFT:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_shift_left(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_shift_left(left, right, output);
            return ruda_saturating_integer_shift_left(left, right, output);
        case RUDA_MIR_POLICY_SHIFT_RIGHT:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION)
                return ruda_checked_integer_shift_right(left, right, output);
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING)
                return ruda_wrapping_integer_shift_right(left, right, output);
            return ruda_saturating_integer_shift_right(left, right, output);
        case RUDA_MIR_POLICY_NEGATE:
            if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION) {
                return ruda_checked_integer_negate(left, output);
            }
            if (instruction->integer_policy == RUDA_MIR_INTEGER_WRAPPING) {
                return ruda_wrapping_integer_negate(left, output);
            }
            return ruda_saturating_integer_negate(left, output);
    }
    return RUDA_CHECKED_INTEGER_OVERFLOW;
}

static RudaMirInterpretStatus ruda_mir_numeric_status(
    RudaCheckedIntegerStatus status
) {
    switch (status) {
        case RUDA_CHECKED_INTEGER_OK: return RUDA_MIR_INTERPRET_OK;
        case RUDA_CHECKED_INTEGER_OVERFLOW: return RUDA_MIR_INTERPRET_OVERFLOW;
        case RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO:
            return RUDA_MIR_INTERPRET_DIVISION_BY_ZERO;
        case RUDA_CHECKED_INTEGER_INVALID_SHIFT:
            return RUDA_MIR_INTERPRET_INVALID_SHIFT;
    }
    return RUDA_MIR_INTERPRET_INVALID_MIR;
}

static RudaMirInterpretStatus ruda_mir_numeric_option(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirExecution *execution,
    bool some,
    RudaMirValue payload,
    RudaMirValue *target
) {
    const RudaMirEnum *layout = ruda_mir_enum(
        function, instruction->enum_id
    );
    if (execution == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (layout == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    return ruda_mir_execution_new_enum(
        execution,
        function,
        layout,
        some ? 0 : 1,
        some ? &payload : NULL,
        some ? 1 : 0,
        &target->as.reference
    );
}

RudaMirInterpretStatus ruda_mir_execute_numeric_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaMirValue *first,
    const RudaMirValue *second,
    RudaMirValue *target,
    RudaMirInterpretOptions options
) {
    RudaIntegerKind source_kind;
    RudaIntegerKind target_kind;
    RudaIntegerValue left;
    RudaIntegerValue right;
    RudaIntegerValue output;
    RudaCheckedIntegerStatus status;
    if (function == NULL || instruction == NULL || first == NULL
        || second == NULL || target == NULL
        || !ruda_mir_numeric_kind(first->type, &source_kind)) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    left = (RudaIntegerValue){source_kind, first->as.integer_bits};
    if (instruction->kind == RUDA_MIR_INTEGER_CONVERT) {
        bool converted;
        RudaMirValue payload;
        if (!ruda_mir_numeric_kind(
                instruction->integer_target_type, &target_kind
            )) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        converted = ruda_integer_convert(left, target_kind, &output);
        if (target->type == instruction->integer_target_type) {
            if (!converted) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            target->as.integer_bits = output.bits;
            return RUDA_MIR_INTERPRET_OK;
        }
        payload.type = instruction->integer_target_type;
        payload.as.integer_bits = converted ? output.bits : 0;
        return ruda_mir_numeric_option(
            function, instruction, options.execution,
            converted, payload, target
        );
    }
    right = (RudaIntegerValue){source_kind, second->as.integer_bits};
    output = (RudaIntegerValue){source_kind, 0};
    status = ruda_mir_numeric_apply(
        instruction, left, right, &output
    );
    if (instruction->integer_policy == RUDA_MIR_INTEGER_CHECKED_OPTION) {
        RudaMirValue payload;
        payload.type = first->type;
        payload.as.integer_bits = status == RUDA_CHECKED_INTEGER_OK
            ? output.bits : 0;
        return ruda_mir_numeric_option(
            function, instruction, options.execution,
            status == RUDA_CHECKED_INTEGER_OK, payload, target
        );
    }
    if (status == RUDA_CHECKED_INTEGER_OK) {
        target->as.integer_bits = output.bits;
    }
    return ruda_mir_numeric_status(status);
}
