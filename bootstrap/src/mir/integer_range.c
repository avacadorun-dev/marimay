#include "mir_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/checked_i64.h"
#include "mmay/base/integer_range.h"

typedef struct MarimayMirIntegerRewrite {
    MarimayMirInstruction *instruction;
    MarimayMirInstructionKind original_kind;
} MarimayMirIntegerRewrite;

static bool mmay_mir_integer_register_bounds(
    const void *context,
    uint32_t register_id,
    MarimayIntegerRange *range
) {
    const MarimayMirFunction *function = context;
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, register_id) : NULL;
    if (type == NULL || range == NULL) {
        return false;
    }
    switch (*type) {
        case MMAY_TYPE_BOOL:
            range->minimum = 0;
            range->maximum = 1;
            break;
        case MMAY_TYPE_I8:
            range->minimum = INT8_MIN;
            range->maximum = INT8_MAX;
            break;
        case MMAY_TYPE_I16:
            range->minimum = INT16_MIN;
            range->maximum = INT16_MAX;
            break;
        case MMAY_TYPE_I32:
            range->minimum = INT32_MIN;
            range->maximum = INT32_MAX;
            break;
        case MMAY_TYPE_U8:
            range->minimum = 0;
            range->maximum = UINT8_MAX;
            break;
        case MMAY_TYPE_U16:
            range->minimum = 0;
            range->maximum = UINT16_MAX;
            break;
        case MMAY_TYPE_U32:
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

static bool mmay_mir_integer_block_length(
    const void *context,
    size_t block_index,
    size_t *instruction_count
) {
    const MarimayMirFunction *function = context;
    const MarimayMirBlock *block = function != NULL
        ? mmay_vector_at_const(&function->blocks, block_index) : NULL;
    if (block == NULL || instruction_count == NULL) {
        return false;
    }
    *instruction_count = block->instructions.length;
    return true;
}

static int64_t mmay_mir_integer_constant(
    MarimayTypeId type,
    uint64_t bits
) {
    switch (type) {
        case MMAY_TYPE_I8:
            return (bits & UINT64_C(0x80)) != 0
                ? (int64_t)(bits & UINT64_C(0xff)) - INT64_C(0x100)
                : (int64_t)(bits & UINT64_C(0xff));
        case MMAY_TYPE_I16:
            return (bits & UINT64_C(0x8000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffff)) - INT64_C(0x10000)
                : (int64_t)(bits & UINT64_C(0xffff));
        case MMAY_TYPE_I32:
            return (bits & UINT64_C(0x80000000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffffffff))
                    - INT64_C(0x100000000)
                : (int64_t)(bits & UINT64_C(0xffffffff));
        case MMAY_TYPE_I64:
            return mmay_i64_from_twos_complement(bits);
        default:
            return bits <= (uint64_t)INT64_MAX ? (int64_t)bits : INT64_MAX;
    }
}

static MarimayIntegerRangeInstructionKind mmay_mir_integer_kind(
    MarimayMirInstructionKind kind
) {
    switch (kind) {
        case MMAY_MIR_CONSTANT_I64:
        case MMAY_MIR_CONSTANT_INTEGER:
            return MMAY_INTEGER_RANGE_CONSTANT;
        case MMAY_MIR_COPY:
            return MMAY_INTEGER_RANGE_COPY;
        case MMAY_MIR_ADD_CHECKED_I64:
            return MMAY_INTEGER_RANGE_ADD_CHECKED;
        case MMAY_MIR_ADD_PROVEN_I64:
            return MMAY_INTEGER_RANGE_ADD_PROVEN;
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
            return MMAY_INTEGER_RANGE_SUBTRACT_CHECKED;
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
            return MMAY_INTEGER_RANGE_SUBTRACT_PROVEN;
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
            return MMAY_INTEGER_RANGE_MULTIPLY_CHECKED;
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
            return MMAY_INTEGER_RANGE_MULTIPLY_PROVEN;
        case MMAY_MIR_REMAINDER_CHECKED_I64:
            return MMAY_INTEGER_RANGE_REMAINDER_CHECKED;
        case MMAY_MIR_INTEGER_CONVERT:
            return MMAY_INTEGER_RANGE_CONVERT_TOTAL;
        case MMAY_MIR_ARRAY_LENGTH:
            return MMAY_INTEGER_RANGE_ARRAY_LENGTH;
        case MMAY_MIR_LESS_I64:
            return MMAY_INTEGER_RANGE_LESS;
        case MMAY_MIR_LESS_EQUAL_I64:
            return MMAY_INTEGER_RANGE_LESS_EQUAL;
        default:
            return MMAY_INTEGER_RANGE_OTHER;
    }
}

static bool mmay_mir_integer_read_instruction(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    MarimayIntegerRangeInstruction *result
) {
    const MarimayMirFunction *function = context;
    const MarimayMirBlock *block = function != NULL
        ? mmay_vector_at_const(&function->blocks, block_index) : NULL;
    const MarimayMirInstruction *instruction = block != NULL
        ? mmay_vector_at_const(
            &block->instructions, instruction_index
        ) : NULL;
    MarimayIntegerRangeInstructionKind kind = instruction != NULL
        ? mmay_mir_integer_kind(instruction->kind)
        : MMAY_INTEGER_RANGE_OTHER;
    const MarimayTypeId *target_type = instruction != NULL
        ? mmay_vector_at_const(
            &function->register_types, instruction->target
        ) : NULL;
    if (instruction == NULL || result == NULL
        || (target_type == NULL && kind != MMAY_INTEGER_RANGE_OTHER)) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->kind = kind;
    result->target = target_type != NULL
        ? instruction->target : MMAY_INTEGER_RANGE_NO_REGISTER;
    result->first = instruction->first;
    result->second = instruction->second;
    result->constant = instruction->kind == MMAY_MIR_CONSTANT_I64
        ? instruction->integer
        : target_type != NULL
            ? mmay_mir_integer_constant(
                *target_type, instruction->integer_bits
            ) : 0;
    return true;
}

static bool mmay_mir_integer_read_terminator(
    const void *context,
    size_t block_index,
    MarimayIntegerRangeTerminator *result
) {
    const MarimayMirFunction *function = context;
    const MarimayMirBlock *block = function != NULL
        ? mmay_vector_at_const(&function->blocks, block_index) : NULL;
    if (block == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->condition = block->terminator.value;
    result->target = block->terminator.target;
    result->otherwise = block->terminator.otherwise;
    if (block->terminator.kind == MMAY_MIR_GOTO) {
        result->kind = MMAY_INTEGER_RANGE_TERMINATOR_GOTO;
    } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
        result->kind = MMAY_INTEGER_RANGE_TERMINATOR_BRANCH;
    } else {
        result->kind = MMAY_INTEGER_RANGE_TERMINATOR_OTHER;
    }
    return true;
}

static MarimayIntegerRangeProgram mmay_mir_integer_program(
    const MarimayMirFunction *function
) {
    MarimayIntegerRangeProgram program;
    memset(&program, 0, sizeof(program));
    program.allocator = function->allocator;
    program.context = function;
    program.block_count = function->blocks.length;
    program.register_count = function->register_types.length;
    program.register_bounds = mmay_mir_integer_register_bounds;
    program.block_length = mmay_mir_integer_block_length;
    program.read_instruction = mmay_mir_integer_read_instruction;
    program.read_terminator = mmay_mir_integer_read_terminator;
    return program;
}

static MarimayMirVerifyStatus mmay_mir_integer_verify_status(
    MarimayIntegerRangeStatus status
) {
    switch (status) {
        case MMAY_INTEGER_RANGE_OK:
            return MMAY_MIR_VERIFY_OK;
        case MMAY_INTEGER_RANGE_ALLOCATION_FAILED:
            return MMAY_MIR_VERIFY_ALLOCATION_FAILED;
        case MMAY_INTEGER_RANGE_LIMIT_REACHED:
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        case MMAY_INTEGER_RANGE_INVALID:
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
}

static MarimayMirRangeOptimizeStatus mmay_mir_integer_optimize_status(
    MarimayMirVerifyStatus status
) {
    switch (status) {
        case MMAY_MIR_VERIFY_OK:
            return MMAY_MIR_RANGE_OPTIMIZE_OK;
        case MMAY_MIR_VERIFY_ALLOCATION_FAILED:
            return MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED;
        case MMAY_MIR_VERIFY_LIMIT_REACHED:
            return MMAY_MIR_RANGE_OPTIMIZE_LIMIT_REACHED;
        default:
            return MMAY_MIR_RANGE_OPTIMIZE_INVALID_MIR;
    }
}

static bool mmay_mir_integer_is_candidate(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayTypeId *type = instruction != NULL
        ? mmay_vector_at_const(
            &function->register_types, instruction->target
        ) : NULL;
    return type != NULL && *type == MMAY_TYPE_I64
        && (instruction->kind == MMAY_MIR_ADD_CHECKED_I64
            || instruction->kind == MMAY_MIR_SUBTRACT_CHECKED_I64
            || instruction->kind == MMAY_MIR_MULTIPLY_CHECKED_I64);
}

static MarimayMirInstructionKind mmay_mir_integer_proven_kind(
    MarimayMirInstructionKind kind
) {
    switch (kind) {
        case MMAY_MIR_ADD_CHECKED_I64:
            return MMAY_MIR_ADD_PROVEN_I64;
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
            return MMAY_MIR_SUBTRACT_PROVEN_I64;
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
            return MMAY_MIR_MULTIPLY_PROVEN_I64;
        default:
            return kind;
    }
}

MarimayMirRangeOptimizeStatus mmay_mir_optimize_integer_ranges(
    MarimayMirFunction *function
) {
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    MarimayMirIntegerRewrite *rewrites = NULL;
    size_t bytes = 0;
    size_t block_index = 0;
    size_t rewrite_count = 0;
    size_t work = 0;
    MarimayIntegerRangeStatus range_status;
    MarimayMirVerifyStatus verify_status = MMAY_MIR_VERIFY_OK;
    bool has_candidate = false;
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || !function->initialized) {
        return MMAY_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            has_candidate = has_candidate || mmay_mir_integer_is_candidate(
                function,
                mmay_vector_at_const(&block->instructions, instruction_index)
            );
        }
    }
    if (!has_candidate) {
        return MMAY_MIR_RANGE_OPTIMIZE_OK;
    }
    verify_status = mmay_mir_verify(function);
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        return mmay_mir_integer_optimize_status(verify_status);
    }
    program = mmay_mir_integer_program(function);
    range_status = mmay_integer_range_analyze(
        &program, function->options.max_verify_work, &work, &analysis
    );
    if (range_status == MMAY_INTEGER_RANGE_INVALID) {
        return MMAY_MIR_RANGE_OPTIMIZE_OK;
    }
    if (range_status != MMAY_INTEGER_RANGE_OK) {
        return mmay_mir_integer_optimize_status(
            mmay_mir_integer_verify_status(range_status)
        );
    }
    if (!mmay_size_multiply(
            function->instruction_count == 0
                ? 1 : function->instruction_count,
            sizeof(*rewrites), &bytes
        )) {
        mmay_integer_range_analysis_dispose(&analysis);
        return MMAY_MIR_RANGE_OPTIMIZE_LIMIT_REACHED;
    }
    rewrites = mmay_allocate_array(
        &function->allocator,
        function->instruction_count == 0 ? 1 : function->instruction_count,
        sizeof(*rewrites)
    );
    if (rewrites == NULL) {
        mmay_integer_range_analysis_dispose(&analysis);
        return MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, block_index);
        size_t instruction_index = 0;
        if (!mmay_integer_range_begin_block(&analysis, block_index)) {
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            MarimayIntegerRangeInstruction normalized;
            if (!mmay_mir_integer_read_instruction(
                    function, block_index, instruction_index, &normalized
                )) {
                verify_status = MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                break;
            }
            if (mmay_mir_integer_is_candidate(function, instruction)
                && mmay_integer_range_proves_instruction(
                    &analysis, &normalized
                )) {
                rewrites[rewrite_count].instruction = instruction;
                rewrites[rewrite_count].original_kind = instruction->kind;
                rewrite_count += 1;
            }
            if (!mmay_integer_range_advance(&analysis, &normalized)) {
                verify_status = MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                break;
            }
        }
        if (verify_status != MMAY_MIR_VERIFY_OK) {
            break;
        }
    }
    if (verify_status == MMAY_MIR_VERIFY_OK) {
        for (block_index = 0; block_index < rewrite_count; block_index += 1) {
            rewrites[block_index].instruction->kind =
                mmay_mir_integer_proven_kind(
                    rewrites[block_index].original_kind
                );
        }
        verify_status = mmay_mir_verify(function);
    }
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        for (block_index = 0; block_index < rewrite_count; block_index += 1) {
            rewrites[block_index].instruction->kind =
                rewrites[block_index].original_kind;
        }
    }
    mmay_deallocate(&function->allocator, rewrites, bytes);
    mmay_integer_range_analysis_dispose(&analysis);
    return mmay_mir_integer_optimize_status(verify_status);
}

MarimayMirVerifyStatus mmay_mir_verify_integer_range_proofs(
    const MarimayMirFunction *function,
    size_t *work
) {
    MarimayIntegerRangeProgram program;
    MarimayIntegerRangeAnalysis analysis;
    size_t analysis_work = 0;
    size_t block_index = 0;
    bool has_proven = false;
    MarimayIntegerRangeStatus range_status;
    MarimayMirVerifyStatus status = MMAY_MIR_VERIFY_OK;
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || work == NULL) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            has_proven = has_proven || (instruction != NULL
                && (instruction->kind == MMAY_MIR_ADD_PROVEN_I64
                    || instruction->kind == MMAY_MIR_SUBTRACT_PROVEN_I64
                    || instruction->kind == MMAY_MIR_MULTIPLY_PROVEN_I64));
        }
    }
    if (!has_proven) {
        return MMAY_MIR_VERIFY_OK;
    }
    if (*work >= function->options.max_verify_work) {
        return MMAY_MIR_VERIFY_LIMIT_REACHED;
    }
    program = mmay_mir_integer_program(function);
    range_status = mmay_integer_range_analyze(
        &program, function->options.max_verify_work - *work,
        &analysis_work, &analysis
    );
    if (range_status != MMAY_INTEGER_RANGE_OK) {
        return mmay_mir_integer_verify_status(range_status);
    }
    *work += analysis_work;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (!mmay_integer_range_begin_block(&analysis, block_index)) {
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const MarimayMirInstruction *instruction = mmay_vector_at_const(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && (instruction->kind == MMAY_MIR_ADD_PROVEN_I64
                        || instruction->kind == MMAY_MIR_SUBTRACT_PROVEN_I64
                        || instruction->kind
                            == MMAY_MIR_MULTIPLY_PROVEN_I64)) {
                    status = MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
                    break;
                }
            }
            if (status != MMAY_MIR_VERIFY_OK) {
                break;
            }
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayIntegerRangeInstruction normalized;
            bool proven = instruction != NULL
                && (instruction->kind == MMAY_MIR_ADD_PROVEN_I64
                    || instruction->kind == MMAY_MIR_SUBTRACT_PROVEN_I64
                    || instruction->kind == MMAY_MIR_MULTIPLY_PROVEN_I64);
            if (!mmay_mir_integer_read_instruction(
                    function, block_index, instruction_index, &normalized
                ) || (proven && !mmay_integer_range_proves_instruction(
                    &analysis, &normalized
                )) || !mmay_integer_range_advance(
                    &analysis, &normalized
                )) {
                status = proven
                    ? MMAY_MIR_VERIFY_INVALID_ARGUMENT
                    : MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
                break;
            }
        }
        if (status != MMAY_MIR_VERIFY_OK) {
            break;
        }
    }
    mmay_integer_range_analysis_dispose(&analysis);
    return status;
}
