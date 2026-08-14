#include "ruda/bytecode/bytecode.h"

#include <limits.h>
#include <string.h>

#include "bytecode_internal.h"

static const RudaBytecodeType *ruda_bytecode_register_type(
    const RudaBytecodeFunction *function,
    uint64_t register_id
) {
    if (function == NULL || register_id >= function->register_types.length) {
        return NULL;
    }
    return ruda_vector_at_const(
        &function->register_types,
        (size_t)register_id
    );
}

const RudaBytecodeInstruction *ruda_bytecode_block_terminator(
    const RudaBytecodeBlock *block
) {
    if (block == NULL || block->instructions.length == 0) {
        return NULL;
    }
    return ruda_vector_at_const(
        &block->instructions,
        block->instructions.length - 1
    );
}

bool ruda_bytecode_opcode_writes_register(RudaOpcode opcode) {
    switch (opcode) {
        case RUDA_OPCODE_CONST_I64:
        case RUDA_OPCODE_CONST_F64:
        case RUDA_OPCODE_CONST_BOOL:
        case RUDA_OPCODE_COPY:
        case RUDA_OPCODE_I64_ADD_CHECKED:
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
        case RUDA_OPCODE_I64_DIVIDE_CHECKED:
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
        case RUDA_OPCODE_I64_NEGATE_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_NOT:
        case RUDA_OPCODE_F64_NEGATE:
        case RUDA_OPCODE_BOOL_NOT:
        case RUDA_OPCODE_I64_EQUAL:
        case RUDA_OPCODE_BOOL_EQUAL:
        case RUDA_OPCODE_I64_LESS:
        case RUDA_OPCODE_I64_LESS_EQUAL:
        case RUDA_OPCODE_I64_BITWISE_AND:
        case RUDA_OPCODE_I64_BITWISE_XOR:
        case RUDA_OPCODE_I64_BITWISE_OR:
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case RUDA_OPCODE_I64_SHIFT_RIGHT:
        case RUDA_OPCODE_F64_ADD:
        case RUDA_OPCODE_F64_SUBTRACT:
        case RUDA_OPCODE_F64_MULTIPLY:
        case RUDA_OPCODE_F64_DIVIDE:
        case RUDA_OPCODE_F64_EQUAL:
        case RUDA_OPCODE_F64_LESS:
        case RUDA_OPCODE_F64_LESS_EQUAL:
        case RUDA_OPCODE_CONST_STRING:
        case RUDA_OPCODE_STRING_EQUAL:
        case RUDA_OPCODE_CALL_DIRECT:
        case RUDA_OPCODE_ARRAY_NEW:
        case RUDA_OPCODE_ARRAY_LENGTH:
        case RUDA_OPCODE_ARRAY_REQUIRE_INDEX:
        case RUDA_OPCODE_ARRAY_GET:
            return true;
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
        case RUDA_OPCODE_CALL_DIRECT_VOID:
        case RUDA_OPCODE_PANIC_STRING:
        case RUDA_OPCODE_CALL_DIRECT_NEVER:
        case RUDA_OPCODE_ARRAY_SET:
            return false;
    }
    return false;
}

void ruda_bytecode_transfer_assignments(
    const RudaBytecodeBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t instruction_index = 0;
    if (block == NULL || assigned == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index + 1 < block->instructions.length;
        instruction_index += 1) {
        const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
            &block->instructions,
            instruction_index
        );
        if (instruction != NULL
            && ruda_bytecode_opcode_writes_register(instruction->opcode)
            && instruction->operands[0].unsigned_value < register_count) {
            assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
        }
    }
}

void ruda_bytecode_transfer_checked_index_instruction(
    const RudaBytecodeInstruction *instruction,
    uint64_t *checked,
    size_t register_count
) {
    uint64_t target = 0;
    size_t register_index = 0;
    if (instruction == NULL || checked == NULL
        || !ruda_bytecode_opcode_writes_register(instruction->opcode)) {
        return;
    }
    target = instruction->operands[0].unsigned_value;
    if (target >= register_count) {
        return;
    }
    for (register_index = 0; register_index < register_count;
        register_index += 1) {
        if (checked[register_index] == target) {
            checked[register_index] = RUDA_BYTECODE_CHECKED_INDEX_NONE;
        }
    }
    checked[(size_t)target] = instruction->opcode
            == RUDA_OPCODE_ARRAY_REQUIRE_INDEX
        ? instruction->operands[2].unsigned_value
        : RUDA_BYTECODE_CHECKED_INDEX_NONE;
}

void ruda_bytecode_transfer_checked_indices(
    const RudaBytecodeBlock *block,
    uint64_t *checked,
    size_t register_count
) {
    size_t instruction_index = 0;
    if (block == NULL || checked == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index + 1 < block->instructions.length;
        instruction_index += 1) {
        const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
            &block->instructions, instruction_index
        );
        ruda_bytecode_transfer_checked_index_instruction(
            instruction, checked, register_count
        );
    }
}

uint64_t ruda_bytecode_meet_checked_index(
    uint64_t left,
    uint64_t right
) {
    if (left == RUDA_BYTECODE_CHECKED_INDEX_TOP) {
        return right;
    }
    if (right == RUDA_BYTECODE_CHECKED_INDEX_TOP || left == right) {
        return left;
    }
    return RUDA_BYTECODE_CHECKED_INDEX_NONE;
}

static RudaBytecodeVerifyStatus ruda_bytecode_require_assigned(
    const unsigned char *assigned,
    size_t register_count,
    uint64_t register_id
) {
    if (register_id >= register_count) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return assigned[(size_t)register_id] != 0
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_call_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    bool returns_void = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID;
    bool returns_never = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER;
    uint64_t call_wire = returns_void || returns_never
        ? instruction->operands[0].unsigned_value
        : instruction->operands[1].unsigned_value;
    const RudaBytecodeCall *call = call_wire < function->calls.length
        ? ruda_vector_at_const(&function->calls, (size_t)call_wire)
        : NULL;
    size_t argument_index = 0;
    if (call == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    if (returns_void != (call->result_type == RUDA_BYTECODE_TYPE_VOID)
        || returns_never != (call->result_type == RUDA_BYTECODE_TYPE_NEVER)) {
        return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (!returns_void && !returns_never) {
        uint64_t target = instruction->operands[0].unsigned_value;
        const RudaBytecodeType *target_type = ruda_bytecode_register_type(
            function,
            target
        );
        if (target_type == NULL) {
            return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
        }
        if (*target_type != call->result_type
            || !ruda_bytecode_type_is_register(*target_type)) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
    }
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const RudaBytecodeCallArgument *argument = ruda_vector_at_const(
            &function->call_arguments,
            call->argument_start + argument_index
        );
        RudaBytecodeVerifyStatus status = argument != NULL
            ? ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                argument->register_id
            )
            : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
    }
    if (!returns_void && !returns_never) {
        assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
    }
    return RUDA_BYTECODE_VERIFY_OK;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_value_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = 0;
    uint64_t second = 0;
    const RudaBytecodeType *target_type = NULL;
    const RudaBytecodeType *first_type = NULL;
    const RudaBytecodeType *second_type = NULL;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (instruction->opcode == RUDA_OPCODE_CALL_DIRECT
        || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID
        || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER) {
        return ruda_bytecode_verify_call_instruction(
            function,
            instruction,
            assigned
        );
    }
    if (instruction->opcode == RUDA_OPCODE_ARRAY_SET) {
        uint64_t array = instruction->operands[0].unsigned_value;
        uint64_t index = instruction->operands[1].unsigned_value;
        uint64_t value = instruction->operands[2].unsigned_value;
        const RudaBytecodeType *array_type;
        const RudaBytecodeType *index_type;
        const RudaBytecodeType *value_type;
        status = ruda_bytecode_require_assigned(
            assigned, function->register_types.length, array
        );
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, index
            );
        }
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, value
            );
        }
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        array_type = ruda_bytecode_register_type(function, array);
        index_type = ruda_bytecode_register_type(function, index);
        value_type = ruda_bytecode_register_type(function, value);
        return array_type != NULL && index_type != NULL && value_type != NULL
                && ruda_bytecode_type_is_array(*array_type)
                && *index_type == RUDA_BYTECODE_TYPE_I64
                && *value_type == ruda_bytecode_array_element_type(*array_type)
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    target_type = ruda_bytecode_register_type(function, target);
    if (target_type == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->opcode) {
        case RUDA_OPCODE_CONST_I64:
            status = *target_type == RUDA_BYTECODE_TYPE_I64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_F64:
            status = *target_type == RUDA_BYTECODE_TYPE_F64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_BOOL:
            if (instruction->operands[1].unsigned_value > 1) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == RUDA_BYTECODE_TYPE_BOOL
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_STRING:
            if (instruction->operands[1].unsigned_value
                    >= function->string_constants.length) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == RUDA_BYTECODE_TYPE_STRING
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_ARRAY_NEW: {
            uint64_t construction_wire = instruction->operands[1].unsigned_value;
            const RudaBytecodeArrayConstruction *construction =
                construction_wire < function->array_constructions.length
                ? ruda_bytecode_array_construction(
                    function,
                    (RudaBytecodeArrayConstructionId)construction_wire
                )
                : NULL;
            size_t argument_index = 0;
            if (construction == NULL
                || *target_type != construction->array_type) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < construction->argument_count;
                argument_index += 1) {
                const RudaBytecodeArrayArgument *argument =
                    ruda_bytecode_array_argument(
                        function, construction->argument_start + argument_index
                    );
                status = argument != NULL
                    ? ruda_bytecode_require_assigned(
                        assigned, function->register_types.length,
                        argument->register_id
                    )
                    : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
                if (status != RUDA_BYTECODE_VERIFY_OK) {
                    return status;
                }
            }
            break;
        }
        case RUDA_OPCODE_ARRAY_LENGTH:
        case RUDA_OPCODE_ARRAY_REQUIRE_INDEX:
        case RUDA_OPCODE_ARRAY_GET: {
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            if (instruction->opcode == RUDA_OPCODE_ARRAY_LENGTH) {
                status = first_type != NULL
                        && ruda_bytecode_type_is_array(*first_type)
                        && *target_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
                break;
            }
            second = instruction->operands[2].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, second
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            second_type = ruda_bytecode_register_type(function, second);
            status = first_type != NULL && second_type != NULL
                    && ruda_bytecode_type_is_array(*first_type)
                    && *second_type == RUDA_BYTECODE_TYPE_I64
                    && (instruction->opcode == RUDA_OPCODE_ARRAY_REQUIRE_INDEX
                        ? *target_type == *first_type
                        : *target_type
                            == ruda_bytecode_array_element_type(*first_type))
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        }
        case RUDA_OPCODE_COPY:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL && *target_type == *first_type
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_I64_NEGATE_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_NOT:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_I64
                    && *first_type == RUDA_BYTECODE_TYPE_I64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_F64_NEGATE:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned, function->register_types.length, first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_F64
                    && *first_type == RUDA_BYTECODE_TYPE_F64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_BOOL_NOT:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_BOOL
                    && *first_type == RUDA_BYTECODE_TYPE_BOOL
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_I64_ADD_CHECKED:
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
        case RUDA_OPCODE_I64_DIVIDE_CHECKED:
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_AND:
        case RUDA_OPCODE_I64_BITWISE_XOR:
        case RUDA_OPCODE_I64_BITWISE_OR:
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case RUDA_OPCODE_I64_SHIFT_RIGHT:
        case RUDA_OPCODE_I64_EQUAL:
        case RUDA_OPCODE_I64_LESS:
        case RUDA_OPCODE_I64_LESS_EQUAL:
        case RUDA_OPCODE_BOOL_EQUAL:
        case RUDA_OPCODE_STRING_EQUAL:
        case RUDA_OPCODE_F64_ADD:
        case RUDA_OPCODE_F64_SUBTRACT:
        case RUDA_OPCODE_F64_MULTIPLY:
        case RUDA_OPCODE_F64_DIVIDE:
        case RUDA_OPCODE_F64_EQUAL:
        case RUDA_OPCODE_F64_LESS:
        case RUDA_OPCODE_F64_LESS_EQUAL:
            first = instruction->operands[1].unsigned_value;
            second = instruction->operands[2].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status == RUDA_BYTECODE_VERIFY_OK) {
                status = ruda_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    second
                );
            }
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            second_type = ruda_bytecode_register_type(function, second);
            if (instruction->opcode == RUDA_OPCODE_BOOL_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_BOOL
                        && *second_type == RUDA_BYTECODE_TYPE_BOOL
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_STRING_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_STRING
                        && *second_type == RUDA_BYTECODE_TYPE_STRING
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_I64_EQUAL
                || instruction->opcode == RUDA_OPCODE_I64_LESS
                || instruction->opcode == RUDA_OPCODE_I64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_I64
                        && *second_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_F64_EQUAL
                || instruction->opcode == RUDA_OPCODE_F64_LESS
                || instruction->opcode == RUDA_OPCODE_F64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_F64
                        && *second_type == RUDA_BYTECODE_TYPE_F64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_F64_ADD
                || instruction->opcode == RUDA_OPCODE_F64_SUBTRACT
                || instruction->opcode == RUDA_OPCODE_F64_MULTIPLY
                || instruction->opcode == RUDA_OPCODE_F64_DIVIDE) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_F64
                        && *first_type == RUDA_BYTECODE_TYPE_F64
                        && *second_type == RUDA_BYTECODE_TYPE_F64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_I64
                        && *first_type == RUDA_BYTECODE_TYPE_I64
                        && *second_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
        case RUDA_OPCODE_CALL_DIRECT:
        case RUDA_OPCODE_CALL_DIRECT_VOID:
        case RUDA_OPCODE_PANIC_STRING:
        case RUDA_OPCODE_CALL_DIRECT_NEVER:
        case RUDA_OPCODE_ARRAY_SET:
            return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
    }
    if (status == RUDA_BYTECODE_VERIFY_OK) {
        assigned[(size_t)target] = 1;
    }
    return status;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_terminator(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    const RudaBytecodeType *type = NULL;
    if (instruction->opcode == RUDA_OPCODE_JUMP) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == RUDA_OPCODE_BRANCH_BOOL) {
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        type = ruda_bytecode_register_type(
            function,
            instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == RUDA_BYTECODE_TYPE_BOOL
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RETURN) {
        if (function->result_type == RUDA_BYTECODE_TYPE_VOID
            || function->result_type == RUDA_BYTECODE_TYPE_NEVER) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        type = ruda_bytecode_register_type(
            function,
            instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == function->result_type
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RETURN_VOID) {
        return function->result_type == RUDA_BYTECODE_TYPE_VOID
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_PANIC_STRING) {
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        type = status == RUDA_BYTECODE_VERIFY_OK
            ? ruda_bytecode_register_type(
                function,
                instruction->operands[0].unsigned_value
            )
            : NULL;
        return status != RUDA_BYTECODE_VERIFY_OK ? status
            : type != NULL && *type == RUDA_BYTECODE_TYPE_STRING
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER) {
        return ruda_bytecode_verify_call_instruction(
            function,
            instruction,
            (unsigned char *)assigned
        );
    }
    return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
}
