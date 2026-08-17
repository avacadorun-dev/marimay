#include "vm_internal.h"

#include "ruda/base/checked_integer.h"

static bool ruda_bytecode_numeric_kind(
    RudaBytecodeType type,
    RudaIntegerKind *kind
) {
    if (kind == NULL) return false;
    switch (type) {
        case RUDA_BYTECODE_TYPE_I8: *kind = RUDA_INTEGER_I8; return true;
        case RUDA_BYTECODE_TYPE_I16: *kind = RUDA_INTEGER_I16; return true;
        case RUDA_BYTECODE_TYPE_I32: *kind = RUDA_INTEGER_I32; return true;
        case RUDA_BYTECODE_TYPE_I64: *kind = RUDA_INTEGER_I64; return true;
        case RUDA_BYTECODE_TYPE_U8: *kind = RUDA_INTEGER_U8; return true;
        case RUDA_BYTECODE_TYPE_U16: *kind = RUDA_INTEGER_U16; return true;
        case RUDA_BYTECODE_TYPE_U32: *kind = RUDA_INTEGER_U32; return true;
        case RUDA_BYTECODE_TYPE_U64: *kind = RUDA_INTEGER_U64; return true;
        default: return false;
    }
}

static RudaBytecodeRunStatus ruda_bytecode_numeric_status(
    RudaCheckedIntegerStatus status
) {
    switch (status) {
        case RUDA_CHECKED_INTEGER_OK: return RUDA_BYTECODE_RUN_OK;
        case RUDA_CHECKED_INTEGER_OVERFLOW: return RUDA_BYTECODE_RUN_OVERFLOW;
        case RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO:
            return RUDA_BYTECODE_RUN_DIVISION_BY_ZERO;
        case RUDA_CHECKED_INTEGER_INVALID_SHIFT:
            return RUDA_BYTECODE_RUN_INVALID_SHIFT;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}

static RudaCheckedIntegerStatus ruda_bytecode_numeric_apply(
    unsigned int policy,
    unsigned int operation,
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *output
) {
    switch (operation) {
        case 0:
            if (policy == 0) return ruda_checked_integer_add(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_add(left, right, output);
            return ruda_saturating_integer_add(left, right, output);
        case 1:
            if (policy == 0) return ruda_checked_integer_subtract(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_subtract(left, right, output);
            return ruda_saturating_integer_subtract(left, right, output);
        case 2:
            if (policy == 0) return ruda_checked_integer_multiply(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_multiply(left, right, output);
            return ruda_saturating_integer_multiply(left, right, output);
        case 3:
            if (policy == 0) return ruda_checked_integer_divide(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_divide(left, right, output);
            return ruda_saturating_integer_divide(left, right, output);
        case 4:
            if (policy == 0) return ruda_checked_integer_remainder(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_remainder(left, right, output);
            return ruda_saturating_integer_remainder(left, right, output);
        case 6:
            if (policy == 0) return ruda_checked_integer_shift_left(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_shift_left(left, right, output);
            return ruda_saturating_integer_shift_left(left, right, output);
        case 7:
            if (policy == 0) return ruda_checked_integer_shift_right(left, right, output);
            if (policy == 1) return ruda_wrapping_integer_shift_right(left, right, output);
            return ruda_saturating_integer_shift_right(left, right, output);
        case 5:
            if (policy == 0) return ruda_checked_integer_negate(left, output);
            if (policy == 1) return ruda_wrapping_integer_negate(left, output);
            return ruda_saturating_integer_negate(left, output);
        default: return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
}

static RudaBytecodeType ruda_bytecode_numeric_option_payload(
    const RudaBytecodeFunction *function,
    uint32_t target
) {
    uint32_t enum_id = ruda_bytecode_register_layout(function, target);
    const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
        function, enum_id
    );
    const RudaBytecodeEnumVariant *variant = layout != NULL
        ? ruda_vector_at_const(
            &function->enum_variants, layout->variant_start
        ) : NULL;
    const RudaBytecodeEnumPayload *payload = variant != NULL
        ? ruda_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + variant->payload_start
        ) : NULL;
    return payload != NULL ? payload->type : RUDA_BYTECODE_TYPE_INVALID;
}

RudaBytecodeRunStatus ruda_bytecode_run_numeric_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function
) {
    uint32_t target_id = (uint32_t)instruction->operands[0].unsigned_value;
    uint32_t first_id = (uint32_t)instruction->operands[1].unsigned_value;
    uint32_t second_id = (uint32_t)instruction->operands[2].unsigned_value;
    RudaIntegerKind source_kind;
    RudaIntegerKind target_kind;
    RudaIntegerValue left;
    RudaIntegerValue right;
    RudaIntegerValue output;
    if (!ruda_bytecode_numeric_kind(registers[first_id].type, &source_kind)) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    left = (RudaIntegerValue){source_kind, registers[first_id].as.integer_bits};
    if (instruction->opcode == RUDA_OPCODE_INTEGER_CONVERT_TOTAL
        || instruction->opcode == RUDA_OPCODE_INTEGER_CONVERT_OPTION) {
        bool option = instruction->opcode == RUDA_OPCODE_INTEGER_CONVERT_OPTION;
        RudaBytecodeType output_type = option
            ? ruda_bytecode_numeric_option_payload(function, target_id)
            : registers[target_id].type;
        bool converted;
        RudaBytecodeValue payload;
        if (!ruda_bytecode_numeric_kind(output_type, &target_kind)) {
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
        }
        converted = ruda_integer_convert(left, target_kind, &output);
        if (!option) {
            if (!converted) return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            registers[target_id].as.integer_bits = output.bits;
            return RUDA_BYTECODE_RUN_OK;
        }
        payload.type = output_type;
        payload.as.integer_bits = converted ? output.bits : 0;
        return ruda_bytecode_execution_new_numeric_option(
            execution,
            function,
            ruda_bytecode_register_layout(function, target_id),
            converted,
            payload,
            &registers[target_id]
        );
    }
    {
        unsigned int offset = (unsigned int)instruction->opcode
            - (unsigned int)RUDA_OPCODE_INTEGER_CHECKED_ADD;
        unsigned int policy = offset / 8U;
        unsigned int operation = offset % 8U;
        RudaCheckedIntegerStatus checked_status;
        RudaBytecodeValue payload;
        right = operation == 5
            ? left
            : (RudaIntegerValue){
                source_kind, registers[second_id].as.integer_bits
            };
        output = (RudaIntegerValue){source_kind, 0};
        checked_status = ruda_bytecode_numeric_apply(
            policy, operation, left, right, &output
        );
        if (policy == 0) {
            payload.type = registers[first_id].type;
            payload.as.integer_bits = checked_status == RUDA_CHECKED_INTEGER_OK
                ? output.bits : 0;
            return ruda_bytecode_execution_new_numeric_option(
                execution,
                function,
                ruda_bytecode_register_layout(function, target_id),
                checked_status == RUDA_CHECKED_INTEGER_OK,
                payload,
                &registers[target_id]
            );
        }
        if (checked_status == RUDA_CHECKED_INTEGER_OK) {
            registers[target_id].as.integer_bits = output.bits;
        }
        return ruda_bytecode_numeric_status(checked_status);
    }
}
