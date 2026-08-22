#include "mir_internal.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/checked_i64.h"
#include "ruda/base/integer_range.h"

typedef struct RudaMirIntegerRewrite {
    RudaMirInstruction *instruction;
    RudaMirInstructionKind original_kind;
} RudaMirIntegerRewrite;

static bool ruda_mir_integer_register_bounds(
    const void *context,
    uint32_t register_id,
    RudaIntegerRange *range
) {
    const RudaMirFunction *function = context;
    const RudaTypeId *type = function != NULL
        ? ruda_vector_at_const(&function->register_types, register_id) : NULL;
    if (type == NULL || range == NULL) {
        return false;
    }
    switch (*type) {
        case RUDA_TYPE_BOOL:
            range->minimum = 0;
            range->maximum = 1;
            break;
        case RUDA_TYPE_I8:
            range->minimum = INT8_MIN;
            range->maximum = INT8_MAX;
            break;
        case RUDA_TYPE_I16:
            range->minimum = INT16_MIN;
            range->maximum = INT16_MAX;
            break;
        case RUDA_TYPE_I32:
            range->minimum = INT32_MIN;
            range->maximum = INT32_MAX;
            break;
        case RUDA_TYPE_U8:
            range->minimum = 0;
            range->maximum = UINT8_MAX;
            break;
        case RUDA_TYPE_U16:
            range->minimum = 0;
            range->maximum = UINT16_MAX;
            break;
        case RUDA_TYPE_U32:
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

static bool ruda_mir_integer_block_length(
    const void *context,
    size_t block_index,
    size_t *instruction_count
) {
    const RudaMirFunction *function = context;
    const RudaMirBlock *block = function != NULL
        ? ruda_vector_at_const(&function->blocks, block_index) : NULL;
    if (block == NULL || instruction_count == NULL) {
        return false;
    }
    *instruction_count = block->instructions.length;
    return true;
}

static int64_t ruda_mir_integer_constant(
    RudaTypeId type,
    uint64_t bits
) {
    switch (type) {
        case RUDA_TYPE_I8:
            return (bits & UINT64_C(0x80)) != 0
                ? (int64_t)(bits & UINT64_C(0xff)) - INT64_C(0x100)
                : (int64_t)(bits & UINT64_C(0xff));
        case RUDA_TYPE_I16:
            return (bits & UINT64_C(0x8000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffff)) - INT64_C(0x10000)
                : (int64_t)(bits & UINT64_C(0xffff));
        case RUDA_TYPE_I32:
            return (bits & UINT64_C(0x80000000)) != 0
                ? (int64_t)(bits & UINT64_C(0xffffffff))
                    - INT64_C(0x100000000)
                : (int64_t)(bits & UINT64_C(0xffffffff));
        case RUDA_TYPE_I64:
            return ruda_i64_from_twos_complement(bits);
        default:
            return bits <= (uint64_t)INT64_MAX ? (int64_t)bits : INT64_MAX;
    }
}

static RudaIntegerRangeInstructionKind ruda_mir_integer_kind(
    RudaMirInstructionKind kind
) {
    switch (kind) {
        case RUDA_MIR_CONSTANT_I64:
        case RUDA_MIR_CONSTANT_INTEGER:
            return RUDA_INTEGER_RANGE_CONSTANT;
        case RUDA_MIR_COPY:
            return RUDA_INTEGER_RANGE_COPY;
        case RUDA_MIR_ADD_CHECKED_I64:
            return RUDA_INTEGER_RANGE_ADD_CHECKED;
        case RUDA_MIR_ADD_PROVEN_I64:
            return RUDA_INTEGER_RANGE_ADD_PROVEN;
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
            return RUDA_INTEGER_RANGE_SUBTRACT_CHECKED;
        case RUDA_MIR_SUBTRACT_PROVEN_I64:
            return RUDA_INTEGER_RANGE_SUBTRACT_PROVEN;
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
            return RUDA_INTEGER_RANGE_MULTIPLY_CHECKED;
        case RUDA_MIR_MULTIPLY_PROVEN_I64:
            return RUDA_INTEGER_RANGE_MULTIPLY_PROVEN;
        case RUDA_MIR_REMAINDER_CHECKED_I64:
            return RUDA_INTEGER_RANGE_REMAINDER_CHECKED;
        case RUDA_MIR_INTEGER_CONVERT:
            return RUDA_INTEGER_RANGE_CONVERT_TOTAL;
        case RUDA_MIR_ARRAY_LENGTH:
            return RUDA_INTEGER_RANGE_ARRAY_LENGTH;
        case RUDA_MIR_LESS_I64:
            return RUDA_INTEGER_RANGE_LESS;
        case RUDA_MIR_LESS_EQUAL_I64:
            return RUDA_INTEGER_RANGE_LESS_EQUAL;
        default:
            return RUDA_INTEGER_RANGE_OTHER;
    }
}

static bool ruda_mir_integer_read_instruction(
    const void *context,
    size_t block_index,
    size_t instruction_index,
    RudaIntegerRangeInstruction *result
) {
    const RudaMirFunction *function = context;
    const RudaMirBlock *block = function != NULL
        ? ruda_vector_at_const(&function->blocks, block_index) : NULL;
    const RudaMirInstruction *instruction = block != NULL
        ? ruda_vector_at_const(
            &block->instructions, instruction_index
        ) : NULL;
    RudaIntegerRangeInstructionKind kind = instruction != NULL
        ? ruda_mir_integer_kind(instruction->kind)
        : RUDA_INTEGER_RANGE_OTHER;
    const RudaTypeId *target_type = instruction != NULL
        ? ruda_vector_at_const(
            &function->register_types, instruction->target
        ) : NULL;
    if (instruction == NULL || result == NULL
        || (target_type == NULL && kind != RUDA_INTEGER_RANGE_OTHER)) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->kind = kind;
    result->target = target_type != NULL
        ? instruction->target : RUDA_INTEGER_RANGE_NO_REGISTER;
    result->first = instruction->first;
    result->second = instruction->second;
    result->constant = instruction->kind == RUDA_MIR_CONSTANT_I64
        ? instruction->integer
        : target_type != NULL
            ? ruda_mir_integer_constant(
                *target_type, instruction->integer_bits
            ) : 0;
    return true;
}

static bool ruda_mir_integer_read_terminator(
    const void *context,
    size_t block_index,
    RudaIntegerRangeTerminator *result
) {
    const RudaMirFunction *function = context;
    const RudaMirBlock *block = function != NULL
        ? ruda_vector_at_const(&function->blocks, block_index) : NULL;
    if (block == NULL || result == NULL) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->condition = block->terminator.value;
    result->target = block->terminator.target;
    result->otherwise = block->terminator.otherwise;
    if (block->terminator.kind == RUDA_MIR_GOTO) {
        result->kind = RUDA_INTEGER_RANGE_TERMINATOR_GOTO;
    } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
        result->kind = RUDA_INTEGER_RANGE_TERMINATOR_BRANCH;
    } else {
        result->kind = RUDA_INTEGER_RANGE_TERMINATOR_OTHER;
    }
    return true;
}

static RudaIntegerRangeProgram ruda_mir_integer_program(
    const RudaMirFunction *function
) {
    RudaIntegerRangeProgram program;
    memset(&program, 0, sizeof(program));
    program.allocator = function->allocator;
    program.context = function;
    program.block_count = function->blocks.length;
    program.register_count = function->register_types.length;
    program.register_bounds = ruda_mir_integer_register_bounds;
    program.block_length = ruda_mir_integer_block_length;
    program.read_instruction = ruda_mir_integer_read_instruction;
    program.read_terminator = ruda_mir_integer_read_terminator;
    return program;
}

static RudaMirVerifyStatus ruda_mir_integer_verify_status(
    RudaIntegerRangeStatus status
) {
    switch (status) {
        case RUDA_INTEGER_RANGE_OK:
            return RUDA_MIR_VERIFY_OK;
        case RUDA_INTEGER_RANGE_ALLOCATION_FAILED:
            return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        case RUDA_INTEGER_RANGE_LIMIT_REACHED:
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        case RUDA_INTEGER_RANGE_INVALID:
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
}

static RudaMirRangeOptimizeStatus ruda_mir_integer_optimize_status(
    RudaMirVerifyStatus status
) {
    switch (status) {
        case RUDA_MIR_VERIFY_OK:
            return RUDA_MIR_RANGE_OPTIMIZE_OK;
        case RUDA_MIR_VERIFY_ALLOCATION_FAILED:
            return RUDA_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED;
        case RUDA_MIR_VERIFY_LIMIT_REACHED:
            return RUDA_MIR_RANGE_OPTIMIZE_LIMIT_REACHED;
        default:
            return RUDA_MIR_RANGE_OPTIMIZE_INVALID_MIR;
    }
}

static bool ruda_mir_integer_is_candidate(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaTypeId *type = instruction != NULL
        ? ruda_vector_at_const(
            &function->register_types, instruction->target
        ) : NULL;
    return type != NULL && *type == RUDA_TYPE_I64
        && (instruction->kind == RUDA_MIR_ADD_CHECKED_I64
            || instruction->kind == RUDA_MIR_SUBTRACT_CHECKED_I64
            || instruction->kind == RUDA_MIR_MULTIPLY_CHECKED_I64);
}

static RudaMirInstructionKind ruda_mir_integer_proven_kind(
    RudaMirInstructionKind kind
) {
    switch (kind) {
        case RUDA_MIR_ADD_CHECKED_I64:
            return RUDA_MIR_ADD_PROVEN_I64;
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
            return RUDA_MIR_SUBTRACT_PROVEN_I64;
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
            return RUDA_MIR_MULTIPLY_PROVEN_I64;
        default:
            return kind;
    }
}

RudaMirRangeOptimizeStatus ruda_mir_optimize_integer_ranges(
    RudaMirFunction *function
) {
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    RudaMirIntegerRewrite *rewrites = NULL;
    size_t bytes = 0;
    size_t block_index = 0;
    size_t rewrite_count = 0;
    size_t work = 0;
    RudaIntegerRangeStatus range_status;
    RudaMirVerifyStatus verify_status = RUDA_MIR_VERIFY_OK;
    bool has_candidate = false;
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || !function->initialized) {
        return RUDA_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            has_candidate = has_candidate || ruda_mir_integer_is_candidate(
                function,
                ruda_vector_at_const(&block->instructions, instruction_index)
            );
        }
    }
    if (!has_candidate) {
        return RUDA_MIR_RANGE_OPTIMIZE_OK;
    }
    verify_status = ruda_mir_verify(function);
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        return ruda_mir_integer_optimize_status(verify_status);
    }
    program = ruda_mir_integer_program(function);
    range_status = ruda_integer_range_analyze(
        &program, function->options.max_verify_work, &work, &analysis
    );
    if (range_status == RUDA_INTEGER_RANGE_INVALID) {
        return RUDA_MIR_RANGE_OPTIMIZE_OK;
    }
    if (range_status != RUDA_INTEGER_RANGE_OK) {
        return ruda_mir_integer_optimize_status(
            ruda_mir_integer_verify_status(range_status)
        );
    }
    if (!ruda_size_multiply(
            function->instruction_count == 0
                ? 1 : function->instruction_count,
            sizeof(*rewrites), &bytes
        )) {
        ruda_integer_range_analysis_dispose(&analysis);
        return RUDA_MIR_RANGE_OPTIMIZE_LIMIT_REACHED;
    }
    rewrites = ruda_allocate_array(
        &function->allocator,
        function->instruction_count == 0 ? 1 : function->instruction_count,
        sizeof(*rewrites)
    );
    if (rewrites == NULL) {
        ruda_integer_range_analysis_dispose(&analysis);
        return RUDA_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        RudaMirBlock *block = ruda_vector_at(&function->blocks, block_index);
        size_t instruction_index = 0;
        if (!ruda_integer_range_begin_block(&analysis, block_index)) {
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            RudaMirInstruction *instruction = ruda_vector_at(
                &block->instructions, instruction_index
            );
            RudaIntegerRangeInstruction normalized;
            if (!ruda_mir_integer_read_instruction(
                    function, block_index, instruction_index, &normalized
                )) {
                verify_status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                break;
            }
            if (ruda_mir_integer_is_candidate(function, instruction)
                && ruda_integer_range_proves_instruction(
                    &analysis, &normalized
                )) {
                rewrites[rewrite_count].instruction = instruction;
                rewrites[rewrite_count].original_kind = instruction->kind;
                rewrite_count += 1;
            }
            if (!ruda_integer_range_advance(&analysis, &normalized)) {
                verify_status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                break;
            }
        }
        if (verify_status != RUDA_MIR_VERIFY_OK) {
            break;
        }
    }
    if (verify_status == RUDA_MIR_VERIFY_OK) {
        for (block_index = 0; block_index < rewrite_count; block_index += 1) {
            rewrites[block_index].instruction->kind =
                ruda_mir_integer_proven_kind(
                    rewrites[block_index].original_kind
                );
        }
        verify_status = ruda_mir_verify(function);
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        for (block_index = 0; block_index < rewrite_count; block_index += 1) {
            rewrites[block_index].instruction->kind =
                rewrites[block_index].original_kind;
        }
    }
    ruda_deallocate(&function->allocator, rewrites, bytes);
    ruda_integer_range_analysis_dispose(&analysis);
    return ruda_mir_integer_optimize_status(verify_status);
}

RudaMirVerifyStatus ruda_mir_verify_integer_range_proofs(
    const RudaMirFunction *function,
    size_t *work
) {
    RudaIntegerRangeProgram program;
    RudaIntegerRangeAnalysis analysis;
    size_t analysis_work = 0;
    size_t block_index = 0;
    bool has_proven = false;
    RudaIntegerRangeStatus range_status;
    RudaMirVerifyStatus status = RUDA_MIR_VERIFY_OK;
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || work == NULL) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            has_proven = has_proven || (instruction != NULL
                && (instruction->kind == RUDA_MIR_ADD_PROVEN_I64
                    || instruction->kind == RUDA_MIR_SUBTRACT_PROVEN_I64
                    || instruction->kind == RUDA_MIR_MULTIPLY_PROVEN_I64));
        }
    }
    if (!has_proven) {
        return RUDA_MIR_VERIFY_OK;
    }
    if (*work >= function->options.max_verify_work) {
        return RUDA_MIR_VERIFY_LIMIT_REACHED;
    }
    program = ruda_mir_integer_program(function);
    range_status = ruda_integer_range_analyze(
        &program, function->options.max_verify_work - *work,
        &analysis_work, &analysis
    );
    if (range_status != RUDA_INTEGER_RANGE_OK) {
        return ruda_mir_integer_verify_status(range_status);
    }
    *work += analysis_work;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (!ruda_integer_range_begin_block(&analysis, block_index)) {
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const RudaMirInstruction *instruction = ruda_vector_at_const(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && (instruction->kind == RUDA_MIR_ADD_PROVEN_I64
                        || instruction->kind == RUDA_MIR_SUBTRACT_PROVEN_I64
                        || instruction->kind
                            == RUDA_MIR_MULTIPLY_PROVEN_I64)) {
                    status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                    break;
                }
            }
            if (status != RUDA_MIR_VERIFY_OK) {
                break;
            }
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaIntegerRangeInstruction normalized;
            bool proven = instruction != NULL
                && (instruction->kind == RUDA_MIR_ADD_PROVEN_I64
                    || instruction->kind == RUDA_MIR_SUBTRACT_PROVEN_I64
                    || instruction->kind == RUDA_MIR_MULTIPLY_PROVEN_I64);
            if (!ruda_mir_integer_read_instruction(
                    function, block_index, instruction_index, &normalized
                ) || (proven && !ruda_integer_range_proves_instruction(
                    &analysis, &normalized
                )) || !ruda_integer_range_advance(
                    &analysis, &normalized
                )) {
                status = proven
                    ? RUDA_MIR_VERIFY_INVALID_ARGUMENT
                    : RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                break;
            }
        }
        if (status != RUDA_MIR_VERIFY_OK) {
            break;
        }
    }
    ruda_integer_range_analysis_dispose(&analysis);
    return status;
}
