#include "mir_internal.h"

#include <string.h>

typedef struct RudaMirRangeRewrite {
    RudaMirInstruction *instruction;
    RudaMirInstructionKind original_kind;
} RudaMirRangeRewrite;

typedef struct RudaMirRangeFact {
    RudaMirRegister index;
} RudaMirRangeFact;

static void ruda_mir_range_optimizer_clear(
    RudaMirRangeFact *facts,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        facts[index].index = RUDA_MIR_NO_REGISTER;
    }
}

static bool ruda_mir_range_optimizer_invalidates(
    RudaMirInstructionKind kind
) {
    return kind == RUDA_MIR_CALL || kind == RUDA_MIR_CALL_INTERFACE
        || kind == RUDA_MIR_ARRAY_SET || kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS
        || kind == RUDA_MIR_ARRAY_PUSH
        || kind == RUDA_MIR_ARRAY_POP || kind == RUDA_MIR_ARRAY_TRUNCATE
        || kind == RUDA_MIR_ARRAY_CLEAR || kind == RUDA_MIR_ARRAY_RESERVE;
}

static RudaMirRangeOptimizeStatus ruda_mir_range_status(
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

static bool ruda_mir_range_has_candidate(const RudaMirFunction *function) {
    size_t block_index = 0;
    if (function == NULL) {
        return false;
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
            if (instruction == NULL || (instruction->kind != RUDA_MIR_ARRAY_GET
                    && instruction->kind != RUDA_MIR_ARRAY_SET)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

static RudaMirRangeOptimizeStatus ruda_mir_optimize_array_ranges(
    RudaMirFunction *function
) {
    RudaMirRangeFact *facts = NULL;
    RudaMirRangeRewrite *rewrites = NULL;
    size_t register_count = 0, rewrite_count = 0, fact_bytes = 0;
    size_t rewrite_bytes = 0, block_index = 0;
    size_t analysis_work = 0;
    RudaMirVerifyStatus verify_status;
    RudaMirRangeAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || !function->initialized) {
        return RUDA_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT;
    }
    if (!ruda_mir_range_has_candidate(function)) {
        return RUDA_MIR_RANGE_OPTIMIZE_OK;
    }
    verify_status = ruda_mir_verify(function);
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        return ruda_mir_range_status(verify_status);
    }
    register_count = function->register_types.length;
    if (!ruda_size_multiply(
            register_count == 0 ? 1 : register_count, sizeof(*facts),
            &fact_bytes
        ) || !ruda_size_multiply(
            function->instruction_count == 0 ? 1 : function->instruction_count,
            sizeof(*rewrites), &rewrite_bytes
        )) {
        return RUDA_MIR_RANGE_OPTIMIZE_LIMIT_REACHED;
    }
    facts = ruda_allocate_array(
        &function->allocator, register_count == 0 ? 1 : register_count,
        sizeof(*facts)
    );
    rewrites = ruda_allocate_array(
        &function->allocator,
        function->instruction_count == 0 ? 1 : function->instruction_count,
        sizeof(*rewrites)
    );
    if (facts == NULL || rewrites == NULL) {
        ruda_deallocate(&function->allocator, facts, fact_bytes);
        ruda_deallocate(&function->allocator, rewrites, rewrite_bytes);
        return RUDA_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED;
    }
    verify_status = ruda_mir_range_analysis_init(
        function, &analysis_work, &analysis
    );
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        ruda_deallocate(&function->allocator, facts, fact_bytes);
        ruda_deallocate(&function->allocator, rewrites, rewrite_bytes);
        return ruda_mir_range_status(verify_status);
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        RudaMirBlock *block = ruda_vector_at(&function->blocks, block_index);
        size_t instruction_index = 0;
        ruda_mir_range_optimizer_clear(facts, register_count);
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            RudaMirInstruction *instruction = ruda_vector_at(
                &block->instructions, instruction_index
            );
            if ((instruction->kind == RUDA_MIR_ARRAY_GET
                    || instruction->kind == RUDA_MIR_ARRAY_SET)
                && facts[instruction->first].index == instruction->second) {
                rewrites[rewrite_count].instruction = instruction;
                rewrites[rewrite_count].original_kind = instruction->kind;
                rewrite_count += 1;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                && ruda_mir_range_assumption_valid(
                    &analysis, function, (RudaMirBlockId)block_index,
                    instruction_index, instruction
                )) {
                rewrites[rewrite_count].instruction = instruction;
                rewrites[rewrite_count].original_kind = instruction->kind;
                rewrite_count += 1;
            }
            if (ruda_mir_range_optimizer_invalidates(instruction->kind)) {
                ruda_mir_range_optimizer_clear(facts, register_count);
                continue;
            }
            if (instruction->target < register_count) {
                size_t fact_index = 0;
                for (fact_index = 0; fact_index < register_count;
                    fact_index += 1) {
                    if (facts[fact_index].index == instruction->target) {
                        facts[fact_index].index = RUDA_MIR_NO_REGISTER;
                    }
                }
                facts[instruction->target].index =
                    instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                        ? instruction->second : RUDA_MIR_NO_REGISTER;
            }
        }
    }
    for (block_index = 0; block_index < rewrite_count; block_index += 1) {
        switch (rewrites[block_index].original_kind) {
            case RUDA_MIR_ARRAY_REQUIRE_INDEX:
                rewrites[block_index].instruction->kind =
                    RUDA_MIR_ARRAY_ASSUME_INDEX;
                break;
            case RUDA_MIR_ARRAY_GET:
                rewrites[block_index].instruction->kind =
                    RUDA_MIR_ARRAY_GET_IN_BOUNDS;
                break;
            case RUDA_MIR_ARRAY_SET:
                rewrites[block_index].instruction->kind =
                    RUDA_MIR_ARRAY_SET_IN_BOUNDS;
                break;
            default:
                verify_status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                break;
        }
        if (verify_status != RUDA_MIR_VERIFY_OK) {
            break;
        }
    }
    if (verify_status == RUDA_MIR_VERIFY_OK) {
        verify_status = ruda_mir_verify(function);
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        for (block_index = 0; block_index < rewrite_count; block_index += 1) {
            rewrites[block_index].instruction->kind =
                rewrites[block_index].original_kind;
        }
    }
    ruda_mir_range_analysis_dispose(&analysis);
    ruda_deallocate(&function->allocator, facts, fact_bytes);
    ruda_deallocate(&function->allocator, rewrites, rewrite_bytes);
    return ruda_mir_range_status(verify_status);
}

RudaMirRangeOptimizeStatus ruda_mir_optimize_ranges(
    RudaMirFunction *function
) {
    RudaMirRangeOptimizeStatus status = ruda_mir_optimize_array_ranges(
        function
    );
    return status == RUDA_MIR_RANGE_OPTIMIZE_OK
        ? ruda_mir_optimize_integer_ranges(function) : status;
}
