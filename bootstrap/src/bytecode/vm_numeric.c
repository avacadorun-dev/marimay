#include "vm_internal.h"

#include "mmay/base/checked_integer.h"

static bool mmay_bytecode_numeric_kind(
    MarimayBytecodeType type,
    MarimayIntegerKind *kind
) {
    if (kind == NULL) return false;
    switch (type) {
        case MMAY_BYTECODE_TYPE_I8: *kind = MMAY_INTEGER_I8; return true;
        case MMAY_BYTECODE_TYPE_I16: *kind = MMAY_INTEGER_I16; return true;
        case MMAY_BYTECODE_TYPE_I32: *kind = MMAY_INTEGER_I32; return true;
        case MMAY_BYTECODE_TYPE_I64: *kind = MMAY_INTEGER_I64; return true;
        case MMAY_BYTECODE_TYPE_U8: *kind = MMAY_INTEGER_U8; return true;
        case MMAY_BYTECODE_TYPE_U16: *kind = MMAY_INTEGER_U16; return true;
        case MMAY_BYTECODE_TYPE_U32: *kind = MMAY_INTEGER_U32; return true;
        case MMAY_BYTECODE_TYPE_U64: *kind = MMAY_INTEGER_U64; return true;
        default: return false;
    }
}

static MarimayBytecodeRunStatus mmay_bytecode_numeric_status(
    MarimayCheckedIntegerStatus status
) {
    switch (status) {
        case MMAY_CHECKED_INTEGER_OK: return MMAY_BYTECODE_RUN_OK;
        case MMAY_CHECKED_INTEGER_OVERFLOW: return MMAY_BYTECODE_RUN_OVERFLOW;
        case MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO:
            return MMAY_BYTECODE_RUN_DIVISION_BY_ZERO;
        case MMAY_CHECKED_INTEGER_INVALID_SHIFT:
            return MMAY_BYTECODE_RUN_INVALID_SHIFT;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}

static MarimayCheckedIntegerStatus mmay_bytecode_numeric_apply(
    unsigned int policy,
    unsigned int operation,
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *output
) {
    switch (operation) {
        case 0:
            if (policy == 0) return mmay_checked_integer_add(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_add(left, right, output);
            return mmay_saturating_integer_add(left, right, output);
        case 1:
            if (policy == 0) return mmay_checked_integer_subtract(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_subtract(left, right, output);
            return mmay_saturating_integer_subtract(left, right, output);
        case 2:
            if (policy == 0) return mmay_checked_integer_multiply(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_multiply(left, right, output);
            return mmay_saturating_integer_multiply(left, right, output);
        case 3:
            if (policy == 0) return mmay_checked_integer_divide(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_divide(left, right, output);
            return mmay_saturating_integer_divide(left, right, output);
        case 4:
            if (policy == 0) return mmay_checked_integer_remainder(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_remainder(left, right, output);
            return mmay_saturating_integer_remainder(left, right, output);
        case 6:
            if (policy == 0) return mmay_checked_integer_shift_left(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_shift_left(left, right, output);
            return mmay_saturating_integer_shift_left(left, right, output);
        case 7:
            if (policy == 0) return mmay_checked_integer_shift_right(left, right, output);
            if (policy == 1) return mmay_wrapping_integer_shift_right(left, right, output);
            return mmay_saturating_integer_shift_right(left, right, output);
        case 5:
            if (policy == 0) return mmay_checked_integer_negate(left, output);
            if (policy == 1) return mmay_wrapping_integer_negate(left, output);
            return mmay_saturating_integer_negate(left, output);
        default: return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
}

static MarimayBytecodeType mmay_bytecode_numeric_option_payload(
    const MarimayBytecodeFunction *function,
    uint32_t target
) {
    uint32_t enum_id = mmay_bytecode_register_layout(function, target);
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, enum_id
    );
    const MarimayBytecodeEnumVariant *variant = layout != NULL
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start
        ) : NULL;
    const MarimayBytecodeEnumPayload *payload = variant != NULL
        ? mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + variant->payload_start
        ) : NULL;
    return payload != NULL ? payload->type : MMAY_BYTECODE_TYPE_INVALID;
}

MarimayBytecodeRunStatus mmay_bytecode_run_numeric_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
) {
    uint32_t target_id = (uint32_t)instruction->operands[0].unsigned_value;
    uint32_t first_id = (uint32_t)instruction->operands[1].unsigned_value;
    uint32_t second_id = (uint32_t)instruction->operands[2].unsigned_value;
    MarimayIntegerKind source_kind;
    MarimayIntegerKind target_kind;
    MarimayIntegerValue left;
    MarimayIntegerValue right;
    MarimayIntegerValue output;
    if (!mmay_bytecode_numeric_kind(registers[first_id].type, &source_kind)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    left = (MarimayIntegerValue){source_kind, registers[first_id].as.integer_bits};
    if (instruction->opcode == MMAY_OPCODE_INTEGER_CONVERT_TOTAL
        || instruction->opcode == MMAY_OPCODE_INTEGER_CONVERT_OPTION) {
        bool option = instruction->opcode == MMAY_OPCODE_INTEGER_CONVERT_OPTION;
        MarimayBytecodeType output_type = option
            ? mmay_bytecode_numeric_option_payload(function, target_id)
            : registers[target_id].type;
        bool converted;
        MarimayBytecodeValue payload;
        if (!mmay_bytecode_numeric_kind(output_type, &target_kind)) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        converted = mmay_integer_convert(left, target_kind, &output);
        if (!option) {
            if (!converted) return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            registers[target_id].as.integer_bits = output.bits;
            return MMAY_BYTECODE_RUN_OK;
        }
        payload.type = output_type;
        payload.as.integer_bits = converted ? output.bits : 0;
        return mmay_bytecode_execution_new_numeric_option(
            execution,
            function,
            mmay_bytecode_register_layout(function, target_id),
            converted,
            payload,
            &registers[target_id]
        );
    }
    {
        unsigned int offset = (unsigned int)instruction->opcode
            - (unsigned int)MMAY_OPCODE_INTEGER_CHECKED_ADD;
        unsigned int policy = offset / 8U;
        unsigned int operation = offset % 8U;
        MarimayCheckedIntegerStatus checked_status;
        MarimayBytecodeValue payload;
        right = operation == 5
            ? left
            : (MarimayIntegerValue){
                source_kind, registers[second_id].as.integer_bits
            };
        output = (MarimayIntegerValue){source_kind, 0};
        checked_status = mmay_bytecode_numeric_apply(
            policy, operation, left, right, &output
        );
        if (policy == 0) {
            payload.type = registers[first_id].type;
            payload.as.integer_bits = checked_status == MMAY_CHECKED_INTEGER_OK
                ? output.bits : 0;
            return mmay_bytecode_execution_new_numeric_option(
                execution,
                function,
                mmay_bytecode_register_layout(function, target_id),
                checked_status == MMAY_CHECKED_INTEGER_OK,
                payload,
                &registers[target_id]
            );
        }
        if (checked_status == MMAY_CHECKED_INTEGER_OK) {
            registers[target_id].as.integer_bits = output.bits;
        }
        return mmay_bytecode_numeric_status(checked_status);
    }
}
