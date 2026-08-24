#include "bytecode_internal.h"

static const MarimayBytecodeType *mmay_bytecode_format_register_type(
    const MarimayBytecodeFunction *function,
    uint64_t register_id
) {
    return function != NULL && register_id < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)register_id)
        : NULL;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_format_assigned(
    const MarimayBytecodeFunction *function,
    const unsigned char *assigned,
    uint64_t register_id
) {
    return function != NULL && assigned != NULL
            && register_id < function->register_types.length
            && assigned[register_id] != 0
        ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_format_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    uint64_t target_id;
    uint64_t first_id;
    uint64_t second_id;
    const MarimayBytecodeType *target;
    const MarimayBytecodeType *first;
    const MarimayBytecodeType *second;
    MarimayBytecodeVerifyStatus status;
    if (function == NULL || instruction == NULL || assigned == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    target_id = instruction->operands[0].unsigned_value;
    first_id = instruction->operands[1].unsigned_value;
    second_id = instruction->operands[2].unsigned_value;
    target = mmay_bytecode_format_register_type(function, target_id);
    first = mmay_bytecode_format_register_type(function, first_id);
    if (target == NULL || first == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    status = mmay_bytecode_format_assigned(function, assigned, first_id);
    if (status != MMAY_BYTECODE_VERIFY_OK) {
        return status;
    }
    if (*target != MMAY_BYTECODE_TYPE_STRING) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_FORMAT_F32:
            return *first == MMAY_BYTECODE_TYPE_F32
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_FORMAT_F64:
            return *first == MMAY_BYTECODE_TYPE_F64
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_FORMAT_BOOL:
            return *first == MMAY_BYTECODE_TYPE_BOOL
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_FORMAT_INTEGER:
            return mmay_bytecode_type_is_integer(*first)
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        case MMAY_OPCODE_STRING_CONCAT:
            second = mmay_bytecode_format_register_type(function, second_id);
            if (second == NULL) {
                return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            status = mmay_bytecode_format_assigned(
                function, assigned, second_id
            );
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                return status;
            }
            return *first == MMAY_BYTECODE_TYPE_STRING
                    && *second == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        default:
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
}
