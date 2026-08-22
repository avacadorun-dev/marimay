#include "bytecode_internal.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/checked_i64.h"
#include "ruda/base/integer_range.h"

static bool ruda_bytecode_integer_register_bounds(
    const void *context,
    uint32_t register_id,
    RudaIntegerRange *range
) {
    const RudaBytecodeFunction *function = context;
    const RudaBytecodeType *type = function != NULL
        ? ruda_vector_at_const(&function->register_types, register_id) : NULL;
    if (type == NULL || range == NULL) {
        return false;
    }
    switch (*type) {
        case RUDA_BYTECODE_TYPE_BOOL:
            range->minimum = 0;
            range->maximum = 1;
            break;
        case RUDA_BYTECODE_TYPE_I8:
            range->minimum = INT8_MIN;
            range->maximum = INT8_MAX;
            break;
        case RUDA_BYTECODE_TYPE_I16:
            range->minimum = INT16_MIN;
            range->maximum = INT16_MAX;
            break;
        case RUDA_BYTECODE_TYPE_I32:
            range->minimum = INT32_MIN;
            range->maximum = INT32_MAX;
            break;
        case RUDA_BYTECODE_TYPE_U8:
            range->minimum = 0;
            range->maximum = UINT8_MAX;
            break;
        case RUDA_BYTECODE_TYPE_U16:
            range->minimum = 0;
            range->maximum = UINT16_MAX;
            break;
        case RUDA_BYTECODE_TYPE_U32:
            range->minimum = 0;
            range->maximum = UINT32_MAX;
            break;
        default:
            range->minimum = INT64_MIN;
            range->maximum = INT64_MAX;
            break;
    }
    return true;
}

static bool ruda_bytecode_integer_block_length(
    const void *context,
    size_t block_index,
    size_t *instruction_count
) {
    const RudaBytecodeFunction *function = context;
    const RudaBytecodeBlock *block = function != NULL
        ? ruda_vector_at_const(&function->blocks, block_index) : NULL;
    if (block == NULL || instruction_count == NULL
        || block->instructions.length == 0) {
        return false;
    }
    *instruction_count = block->instructions.length - 1;
    return true;
}

static int64_t ruda_bytecode_integer_constant(
    RudaBytecodeType type,
    uint64_t bits
) {
    switch (type) {
        case RUDA_BYTECODE_TYPE_I8:
            return (bits & UINT64_C(0x80)) != 0
                ? (int64_t)(bits & UINT64_C(0xff)) - INT64_C(0x100)
                : (int64_t)(bits & UINT64_C(0xff));
        case RUDA_BYTECODE_TYPE_I16:
            return (bits & UINT64_C(0x8000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffff)) - INT64_C(0x10000)
                : (int64_t)(bits & UINT64_C(0xffff));
        case RUDA_BYTECODE_TYPE_I32:
            return (bits & UINT64_C(0x80000000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffffffff))
                    - INT64_C(0x100000000)
                : (int64_t)(bits & UINT64_C(0xffffffff));
        case RUDA_BYTECODE_TYPE_I64:
            return ruda_i64_from_twos_complement(bits);
        default:
            return bits <= (uint64_t)INT64_MAX ? (int64_t)bits : INT64_MAX;
    }
}

static RudaIntegerRangeInstructionKind ruda_bytecode_integer_kind(
    RudaOpcode opcode
) {
    switch (opcode) {
        case RUDA_OPCODE_CONST_I64:
        case RUDA_OPCODE_CONST_INTEGER:
            return RUDA_INTEGER_RANGE_CONSTANT;
        case RUDA_OPCODE_COPY:
            return RUDA_INTEGER_RANGE_COPY;
        case RUDA_OPCODE_I64_ADD_CHECKED:
            return RUDA_INTEGER_RANGE_ADD_CHECKED;
        case RUDA_OPCODE_I64_ADD_PROVEN:
            return RUDA_INTEGER_RANGE_ADD_PROVEN;
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
            return RUDA_INTEGER_RANGE_SUBTRACT_CHECKED;
        case RUDA_OPCODE_I64_SUBTRACT_PROVEN:
            return RUDA_INTEGER_RANGE_SUBTRACT_PROVEN;
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
            return RUDA_INTEGER_RANGE_MULTIPLY_CHECKED;
        case RUDA_OPCODE_I64_MULTIPLY_PROVEN:
            return RUDA_INTEGER_RANGE_MULTIPLY_PROVEN;
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
            return RUDA_INTEGER_RANGE_REMAINDER_CHECKED;
        case RUDA_OPCODE_INTEGER_CONVERT_TOTAL:
            return RUDA_INTEGER_RANGE_CONVERT_TOTAL;
        case RUDA_OPCODE_ARRAY_LENGTH:
            return RUDA_INTEGER_RANGE_ARRAY_LENGTH;
        case RUDA_OPCODE_I64_LESS:
            return RUDA_INTEGER_RANGE_LESS;
        case RUDA_OPCODE_I64_LESS_EQUAL:
            return RUDA_INTEGER_RANGE_LESS_EQUAL;
        default:
            return RUDA_INTEGER_RANGE_OTHER;
    }
}

static bool ruda_bytecode_integer_read_instruction(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    RudaIntegerRangeInstruction *result
) {
    const RudaBytecodeFunction *function = context;
    const RudaBytecodeBlock *block = function != NULL
        ? ruda_vector_at_const(&function->blocks, block_index) : NULL;
    const RudaBytecodeInstruction *instruction = block != NULL
        ? ruda_vector_at_const(
            &block->instructions, instruction_index
        ) : NULL;
    uint64_t target = instruction != NULL
        ? instruction->operands[0].unsigned_value : UINT64_MAX;
    const RudaBytecodeType *target_type = target < function->register_types.length
        ? ruda_vector_at_const(&function->register_types, (size_t)target)
        : NULL;
    if (instruction == NULL || target_type == NULL || result == NULL
        || target > UINT32_MAX) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->kind = ruda_bytecode_integer_kind(instruction->opcode);
    result->target = (uint32_t)target;
    result->first = (uint32_t)instruction->operands[1].unsigned_value;
    result->second = (uint32_t)instruction->operands[2].unsigned_value;
    result->constant = instruction->opcode == RUDA_OPCODE_CONST_I64
        ? instruction->operands[1].signed_value
        : ruda_bytecode_integer_constant(
            *target_type, instruction->operands[1].unsigned_value
        );
    return true;
}

static bool ruda_bytecode_integer_read_terminator(
    const void *context,
    size_t block_index,
    RudaIntegerRangeTerminator *result
) {
    const RudaBytecodeFunction *function = context;
    const RudaBytecodeBlock *block = function != NULL
        ? ruda_vector_at_const(&function->blocks, block_index) : NULL;
    const RudaBytecodeInstruction *terminator =
        ruda_bytecode_block_terminator(block);
    if (terminator == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    if (terminator->opcode == RUDA_OPCODE_JUMP) {
        result->kind = RUDA_INTEGER_RANGE_TERMINATOR_GOTO;
        result->target = (uint32_t)terminator->operands[0].unsigned_value;
    } else if (terminator->opcode == RUDA_OPCODE_BRANCH_BOOL) {
        result->kind = RUDA_INTEGER_RANGE_TERMINATOR_BRANCH;
        result->condition = (uint32_t)terminator->operands[0].unsigned_value;
        result->target = (uint32_t)terminator->operands[1].unsigned_value;
        result->otherwise = (uint32_t)terminator->operands[2].unsigned_value;
    } else {
        result->kind = RUDA_INTEGER_RANGE_TERMINATOR_OTHER;
    }
    return true;
}

static RudaBytecodeVerifyStatus ruda_bytecode_integer_range_status(
    RudaIntegerRangeStatus status
) {
    switch (status) {
        case RUDA_INTEGER_RANGE_OK:
            return RUDA_BYTECODE_VERIFY_OK;
        case RUDA_INTEGER_RANGE_ALLOCATION_FAILED:
            return RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
        case RUDA_INTEGER_RANGE_LIMIT_REACHED:
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        case RUDA_INTEGER_RANGE_INVALID:
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

static bool ruda_bytecode_integer_proven(RudaOpcode opcode) {
    return opcode == RUDA_OPCODE_I64_ADD_PROVEN
        || opcode == RUDA_OPCODE_I64_SUBTRACT_PROVEN
        || opcode == RUDA_OPCODE_I64_MULTIPLY_PROVEN;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_integer_range_proofs(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
) {
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    size_t block_index = 0;
    bool has_proven = false;
    RudaIntegerRangeStatus range_status;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (work_used != NULL) {
        *work_used = 0;
    }
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || work_used == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            has_proven = has_proven || (instruction != NULL
                && ruda_bytecode_integer_proven(instruction->opcode));
        }
    }
    if (!has_proven) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    memset(&program, 0, sizeof(program));
    program.allocator = function->allocator;
    program.context = function;
    program.block_count = function->blocks.length;
    program.register_count = function->register_types.length;
    program.register_bounds = ruda_bytecode_integer_register_bounds;
    program.block_length = ruda_bytecode_integer_block_length;
    program.read_instruction = ruda_bytecode_integer_read_instruction;
    program.read_terminator = ruda_bytecode_integer_read_terminator;
    range_status = ruda_integer_range_analyze(
        &program, available_work, work_used, &analysis
    );
    if (range_status != RUDA_INTEGER_RANGE_OK) {
        return ruda_bytecode_integer_range_status(range_status);
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (!ruda_integer_range_begin_block(&analysis, block_index)) {
            for (instruction_index = 0; block != NULL
                && instruction_index + 1 < block->instructions.length;
                instruction_index += 1) {
                const RudaBytecodeInstruction *instruction =
                    ruda_vector_at_const(
                        &block->instructions, instruction_index
                    );
                if (instruction != NULL
                    && ruda_bytecode_integer_proven(instruction->opcode)) {
                    status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                    break;
                }
            }
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                break;
            }
            continue;
        }
        for (instruction_index = 0;
            instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaIntegerRangeInstruction normalized;
            bool proven = instruction != NULL
                && ruda_bytecode_integer_proven(instruction->opcode);
            if (!ruda_bytecode_integer_read_instruction(
                    function, block_index, instruction_index, &normalized
                ) || (proven && !ruda_integer_range_proves_instruction(
                    &analysis, &normalized
                )) || !ruda_integer_range_advance(
                    &analysis, &normalized
                )) {
                status = proven
                    ? RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT
                    : RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                break;
            }
        }
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            break;
        }
    }
    ruda_integer_range_analysis_dispose(&analysis);
    return status;
}
