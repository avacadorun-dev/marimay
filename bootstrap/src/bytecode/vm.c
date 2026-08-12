#include "ruda/bytecode/vm.h"

#include <string.h>

#include "ruda/base/checked_i64.h"

RudaBytecodeRunOptions ruda_bytecode_run_default_options(void) {
    RudaBytecodeRunOptions options;
    options.max_steps = 10000000;
    options.remaining_shared_steps = NULL;
    options.trap = NULL;
    return options;
}

static RudaBytecodeRunStatus ruda_bytecode_run_checked_status(
    RudaCheckedI64Status status
) {
    if (status == RUDA_CHECKED_I64_OK) {
        return RUDA_BYTECODE_RUN_OK;
    }
    if (status == RUDA_CHECKED_I64_DIVISION_BY_ZERO) {
        return RUDA_BYTECODE_RUN_DIVISION_BY_ZERO;
    }
    return RUDA_BYTECODE_RUN_OVERFLOW;
}

static RudaBytecodeRunStatus ruda_bytecode_run_value_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    switch (instruction->opcode) {
        case RUDA_OPCODE_CONST_I64:
            registers[target].as.i64 = instruction->operands[1].signed_value;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_CONST_BOOL:
            registers[target].as.boolean = instruction->operands[1].unsigned_value
                != 0;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_COPY:
            registers[target] = registers[
                (size_t)instruction->operands[1].unsigned_value
            ];
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_I64_ADD_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_add(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_subtract(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_multiply(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_DIVIDE_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_divide(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_REMAINDER_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_remainder(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_NEGATE_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_negate(
                registers[first].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_BOOL_NOT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            registers[target].as.boolean = !registers[first].as.boolean;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.i64
                == registers[second].as.i64;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_BOOL_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.boolean
                == registers[second].as.boolean;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_LESS: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.i64
                < registers[second].as.i64;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_LESS_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.i64
                <= registers[second].as.i64;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}

static bool ruda_bytecode_run_consume_step(
    size_t *steps,
    RudaBytecodeRunOptions options
) {
    if (*steps >= options.max_steps
        || (options.remaining_shared_steps != NULL
            && *options.remaining_shared_steps == 0)) {
        return false;
    }
    *steps += 1;
    if (options.remaining_shared_steps != NULL) {
        *options.remaining_shared_steps -= 1;
    }
    return true;
}

RudaBytecodeRunStatus ruda_bytecode_run(
    const RudaBytecodeFunction *function,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
) {
    RudaBytecodeValue *registers = NULL;
    RudaBytecodeValue completed;
    size_t register_count = 0;
    size_t register_bytes = 0;
    size_t index = 0;
    size_t steps = 0;
    size_t current_block = 0;
    RudaBytecodeRunStatus status = RUDA_BYTECODE_RUN_OK;
    if (options.trap != NULL) {
        memset(options.trap, 0, sizeof(*options.trap));
    }
    if (function == NULL || !function->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || options.max_steps == 0
        || argument_count != function->parameter_count
        || (arguments == NULL && argument_count != 0)) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    {
        RudaBytecodeVerifyStatus verify_status = ruda_bytecode_verify(function);
        if (verify_status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        if (verify_status == RUDA_BYTECODE_VERIFY_LIMIT_REACHED) {
            return RUDA_BYTECODE_RUN_LIMIT_REACHED;
        }
        if (verify_status != RUDA_BYTECODE_VERIFY_OK) {
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
        }
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *expected = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (expected == NULL || arguments[index].type != *expected) {
            return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
        }
    }
    register_count = function->register_types.length;
    if (!ruda_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(RudaBytecodeValue),
            &register_bytes
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    registers = ruda_allocate(&allocator, register_bytes);
    if (registers == NULL) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(registers, 0, register_bytes);
    for (index = 0; index < register_count; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            index
        );
        registers[index].type = *type;
    }
    for (index = 0; index < argument_count; index += 1) {
        registers[index] = arguments[index];
    }
    memset(&completed, 0, sizeof(completed));
    for (;;) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            current_block
        );
        bool transferred = false;
        for (index = 0; index < block->instructions.length; index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                index
            );
            if (!ruda_bytecode_run_consume_step(&steps, options)) {
                status = RUDA_BYTECODE_RUN_STEP_LIMIT;
                if (options.trap != NULL) {
                    options.trap->span = instruction->span;
                    options.trap->has_span = true;
                }
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_RETURN) {
                completed = registers[
                    (size_t)instruction->operands[0].unsigned_value
                ];
                *result = completed;
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_RETURN_VOID) {
                completed.type = RUDA_BYTECODE_TYPE_VOID;
                *result = completed;
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_JUMP) {
                current_block = (size_t)instruction->operands[0].unsigned_value;
                transferred = true;
                break;
            }
            if (instruction->opcode == RUDA_OPCODE_BRANCH_BOOL) {
                size_t condition = (size_t)instruction->operands[0].unsigned_value;
                current_block = (size_t)instruction->operands[
                    registers[condition].as.boolean ? 1 : 2
                ].unsigned_value;
                transferred = true;
                break;
            }
            status = ruda_bytecode_run_value_instruction(
                instruction,
                registers
            );
            if (status != RUDA_BYTECODE_RUN_OK) {
                if (options.trap != NULL
                    && (status == RUDA_BYTECODE_RUN_OVERFLOW
                        || status == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO)) {
                    options.trap->span = instruction->span;
                    options.trap->has_span = true;
                }
                goto cleanup;
            }
        }
        if (!transferred) {
            status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            break;
        }
    }

cleanup:
    ruda_deallocate(&allocator, registers, register_bytes);
    return status;
}
