#include "bytecode_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/checked_i64.h"
#include "mmay/base/integer_range.h"

static bool mmay_bytecode_integer_register_bounds(
    const void *context,
    uint32_t register_id,
    MarimayIntegerRange *range
) {
    const MarimayBytecodeFunction *function = context;
    const MarimayBytecodeType *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, register_id) : NULL;
    if (type == NULL || range == NULL) {
        return false;
    }
    switch (*type) {
        case MMAY_BYTECODE_TYPE_BOOL:
            range->minimum = 0;
            range->maximum = 1;
            break;
        case MMAY_BYTECODE_TYPE_I8:
            range->minimum = INT8_MIN;
            range->maximum = INT8_MAX;
            break;
        case MMAY_BYTECODE_TYPE_I16:
            range->minimum = INT16_MIN;
            range->maximum = INT16_MAX;
            break;
        case MMAY_BYTECODE_TYPE_I32:
            range->minimum = INT32_MIN;
            range->maximum = INT32_MAX;
            break;
        case MMAY_BYTECODE_TYPE_U8:
            range->minimum = 0;
            range->maximum = UINT8_MAX;
            break;
        case MMAY_BYTECODE_TYPE_U16:
            range->minimum = 0;
            range->maximum = UINT16_MAX;
            break;
        case MMAY_BYTECODE_TYPE_U32:
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

static bool mmay_bytecode_integer_block_length(
    const void *context,
    size_t block_index,
    size_t *instruction_count
) {
    const MarimayBytecodeFunction *function = context;
    const MarimayBytecodeBlock *block = function != NULL
        ? mmay_vector_at_const(&function->blocks, block_index) : NULL;
    if (block == NULL || instruction_count == NULL
        || block->instructions.length == 0) {
        return false;
    }
    *instruction_count = block->instructions.length - 1;
    return true;
}

static int64_t mmay_bytecode_integer_constant(
    MarimayBytecodeType type,
    uint64_t bits
) {
    switch (type) {
        case MMAY_BYTECODE_TYPE_I8:
            return (bits & UINT64_C(0x80)) != 0
                ? (int64_t)(bits & UINT64_C(0xff)) - INT64_C(0x100)
                : (int64_t)(bits & UINT64_C(0xff));
        case MMAY_BYTECODE_TYPE_I16:
            return (bits & UINT64_C(0x8000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffff)) - INT64_C(0x10000)
                : (int64_t)(bits & UINT64_C(0xffff));
        case MMAY_BYTECODE_TYPE_I32:
            return (bits & UINT64_C(0x80000000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffffffff))
                    - INT64_C(0x100000000)
                : (int64_t)(bits & UINT64_C(0xffffffff));
        case MMAY_BYTECODE_TYPE_I64:
            return mmay_i64_from_twos_complement(bits);
        default:
            return bits <= (uint64_t)INT64_MAX ? (int64_t)bits : INT64_MAX;
    }
}

static MarimayIntegerRangeInstructionKind mmay_bytecode_integer_kind(
    MarimayOpcode opcode
) {
    switch (opcode) {
        case MMAY_OPCODE_CONST_I64:
        case MMAY_OPCODE_CONST_INTEGER:
            return MMAY_INTEGER_RANGE_CONSTANT;
        case MMAY_OPCODE_COPY:
            return MMAY_INTEGER_RANGE_COPY;
        case MMAY_OPCODE_I64_ADD_CHECKED:
            return MMAY_INTEGER_RANGE_ADD_CHECKED;
        case MMAY_OPCODE_I64_ADD_PROVEN:
            return MMAY_INTEGER_RANGE_ADD_PROVEN;
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED:
            return MMAY_INTEGER_RANGE_SUBTRACT_CHECKED;
        case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
            return MMAY_INTEGER_RANGE_SUBTRACT_PROVEN;
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED:
            return MMAY_INTEGER_RANGE_MULTIPLY_CHECKED;
        case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
            return MMAY_INTEGER_RANGE_MULTIPLY_PROVEN;
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
            return MMAY_INTEGER_RANGE_REMAINDER_CHECKED;
        case MMAY_OPCODE_INTEGER_CONVERT_TOTAL:
            return MMAY_INTEGER_RANGE_CONVERT_TOTAL;
        case MMAY_OPCODE_ARRAY_LENGTH:
            return MMAY_INTEGER_RANGE_ARRAY_LENGTH;
        case MMAY_OPCODE_I64_LESS:
            return MMAY_INTEGER_RANGE_LESS;
        case MMAY_OPCODE_I64_LESS_EQUAL:
            return MMAY_INTEGER_RANGE_LESS_EQUAL;
        default:
            return MMAY_INTEGER_RANGE_OTHER;
    }
}

static bool mmay_bytecode_integer_read_instruction(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    MarimayIntegerRangeInstruction *result
) {
    const MarimayBytecodeFunction *function = context;
    const MarimayBytecodeBlock *block = function != NULL
        ? mmay_vector_at_const(&function->blocks, block_index) : NULL;
    const MarimayBytecodeInstruction *instruction = block != NULL
        ? mmay_vector_at_const(
            &block->instructions, instruction_index
        ) : NULL;
    uint64_t target;
    const MarimayBytecodeType *target_type;
    if (instruction == NULL || result == NULL) return false;
    memset(result, 0, sizeof(*result));
    result->kind = mmay_bytecode_integer_kind(instruction->opcode);
    result->target = MMAY_INTEGER_RANGE_NO_REGISTER;
    /* A void call's first operand is a call-table index, not a destination.
     * Stores and other non-writing opcodes likewise preserve scalar facts. */
    if (!mmay_bytecode_opcode_writes_register(instruction->opcode)) {
        return result->kind == MMAY_INTEGER_RANGE_OTHER;
    }
    target = instruction->operands[0].unsigned_value;
    target_type = target < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)target)
        : NULL;
    if (target_type == NULL || target > UINT32_MAX) {
        return false;
    }
    result->target = (uint32_t)target;
    result->first = (uint32_t)instruction->operands[1].unsigned_value;
    result->second = (uint32_t)instruction->operands[2].unsigned_value;
    result->constant = instruction->opcode == MMAY_OPCODE_CONST_I64
        ? instruction->operands[1].signed_value
        : mmay_bytecode_integer_constant(
            *target_type, instruction->operands[1].unsigned_value
        );
    return true;
}

static bool mmay_bytecode_integer_read_terminator(
    const void *context,
    size_t block_index,
    MarimayIntegerRangeTerminator *result
) {
    const MarimayBytecodeFunction *function = context;
    const MarimayBytecodeBlock *block = function != NULL
        ? mmay_vector_at_const(&function->blocks, block_index) : NULL;
    const MarimayBytecodeInstruction *terminator =
        mmay_bytecode_block_terminator(block);
    if (terminator == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    if (terminator->opcode == MMAY_OPCODE_JUMP) {
        result->kind = MMAY_INTEGER_RANGE_TERMINATOR_GOTO;
        result->target = (uint32_t)terminator->operands[0].unsigned_value;
    } else if (terminator->opcode == MMAY_OPCODE_BRANCH_BOOL) {
        result->kind = MMAY_INTEGER_RANGE_TERMINATOR_BRANCH;
        result->condition = (uint32_t)terminator->operands[0].unsigned_value;
        result->target = (uint32_t)terminator->operands[1].unsigned_value;
        result->otherwise = (uint32_t)terminator->operands[2].unsigned_value;
    } else {
        result->kind = MMAY_INTEGER_RANGE_TERMINATOR_OTHER;
    }
    return true;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_integer_range_status(
    MarimayIntegerRangeStatus status
) {
    switch (status) {
        case MMAY_INTEGER_RANGE_OK:
            return MMAY_BYTECODE_VERIFY_OK;
        case MMAY_INTEGER_RANGE_ALLOCATION_FAILED:
            return MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED;
        case MMAY_INTEGER_RANGE_LIMIT_REACHED:
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        case MMAY_INTEGER_RANGE_INVALID:
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

static bool mmay_bytecode_integer_proven(MarimayOpcode opcode) {
    return opcode == MMAY_OPCODE_I64_ADD_PROVEN
        || opcode == MMAY_OPCODE_I64_SUBTRACT_PROVEN
        || opcode == MMAY_OPCODE_I64_MULTIPLY_PROVEN;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_integer_range_proofs(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
) {
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    size_t block_index = 0;
    bool has_proven = false;
    MarimayIntegerRangeStatus range_status;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (work_used != NULL) {
        *work_used = 0;
    }
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || work_used == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            has_proven = has_proven || (instruction != NULL
                && mmay_bytecode_integer_proven(instruction->opcode));
        }
    }
    if (!has_proven) {
        return MMAY_BYTECODE_VERIFY_OK;
    }
    memset(&program, 0, sizeof(program));
    program.allocator = function->allocator;
    program.context = function;
    program.block_count = function->blocks.length;
    program.register_count = function->register_types.length;
    program.register_bounds = mmay_bytecode_integer_register_bounds;
    program.block_length = mmay_bytecode_integer_block_length;
    program.read_instruction = mmay_bytecode_integer_read_instruction;
    program.read_terminator = mmay_bytecode_integer_read_terminator;
    range_status = mmay_integer_range_analyze(
        &program, available_work, work_used, &analysis
    );
    if (range_status != MMAY_INTEGER_RANGE_OK) {
        return mmay_bytecode_integer_range_status(range_status);
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (!mmay_integer_range_begin_block(&analysis, block_index)) {
            for (instruction_index = 0; block != NULL
                && instruction_index + 1 < block->instructions.length;
                instruction_index += 1) {
                const MarimayBytecodeInstruction *instruction =
                    mmay_vector_at_const(
                        &block->instructions, instruction_index
                    );
                if (instruction != NULL
                    && mmay_bytecode_integer_proven(instruction->opcode)) {
                    status = MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                    break;
                }
            }
            if (status != MMAY_BYTECODE_VERIFY_OK) {
                break;
            }
            continue;
        }
        for (instruction_index = 0;
            instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayIntegerRangeInstruction normalized;
            bool proven = instruction != NULL
                && mmay_bytecode_integer_proven(instruction->opcode);
            if (!mmay_bytecode_integer_read_instruction(
                    function, block_index, instruction_index, &normalized
                ) || (proven && !mmay_integer_range_proves_instruction(
                    &analysis, &normalized
                )) || !mmay_integer_range_advance(
                    &analysis, &normalized
                )) {
                status = proven
                    ? MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT
                    : MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                break;
            }
        }
        if (status != MMAY_BYTECODE_VERIFY_OK) {
            break;
        }
    }
    mmay_integer_range_analysis_dispose(&analysis);
    return status;
}
