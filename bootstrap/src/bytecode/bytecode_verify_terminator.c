#include "bytecode_internal.h"

static const MarimayBytecodeType *mmay_bytecode_terminator_register_type(
    const MarimayBytecodeFunction *function,
    uint64_t register_id
) {
    return function != NULL && register_id < function->register_types.length
        ? mmay_vector_at_const(
            &function->register_types, (size_t)register_id
        )
        : NULL;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_terminator_assigned(
    const unsigned char *assigned,
    size_t register_count,
    uint64_t register_id
) {
    if (assigned == NULL || register_id >= register_count) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return assigned[register_id] != 0
        ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_terminator(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    const MarimayBytecodeType *type = NULL;
    if (instruction->opcode == MMAY_OPCODE_JUMP) {
        return MMAY_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_BRANCH_BOOL) {
        status = mmay_bytecode_terminator_assigned(
            assigned, function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) return status;
        type = mmay_bytecode_terminator_register_type(
            function, instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == MMAY_BYTECODE_TYPE_BOOL
            ? MMAY_BYTECODE_VERIFY_OK
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_RETURN) {
        if (function->result_type == MMAY_BYTECODE_TYPE_VOID
            || function->result_type == MMAY_BYTECODE_TYPE_NEVER) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        status = mmay_bytecode_terminator_assigned(
            assigned, function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != MMAY_BYTECODE_VERIFY_OK) return status;
        type = mmay_bytecode_terminator_register_type(
            function, instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == function->result_type
            ? MMAY_BYTECODE_VERIFY_OK
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_RETURN_VOID) {
        return function->result_type == MMAY_BYTECODE_TYPE_VOID
            ? MMAY_BYTECODE_VERIFY_OK
            : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_PANIC_STRING) {
        status = mmay_bytecode_terminator_assigned(
            assigned, function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        type = status == MMAY_BYTECODE_VERIFY_OK
            ? mmay_bytecode_terminator_register_type(
                function, instruction->operands[0].unsigned_value
            ) : NULL;
        return status != MMAY_BYTECODE_VERIFY_OK ? status
            : type != NULL && *type == MMAY_BYTECODE_TYPE_STRING
                ? MMAY_BYTECODE_VERIFY_OK
                : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER) {
        return mmay_bytecode_verify_call_instruction(
            function, instruction, (unsigned char *)assigned
        );
    }
    return MMAY_BYTECODE_VERIFY_INVALID_OPCODE;
}
