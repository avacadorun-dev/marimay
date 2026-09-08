#include "mir_internal.h"

#include <string.h>

typedef struct MarimayMirRangeRewrite {
    MarimayMirInstruction *instruction;
    MarimayMirInstructionKind original_kind;
} MarimayMirRangeRewrite;

typedef struct MarimayMirRangeFact {
    MarimayMirRegister index;
} MarimayMirRangeFact;

static void mmay_mir_range_optimizer_clear(
    MarimayMirRangeFact *facts,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        facts[index].index = MMAY_MIR_NO_REGISTER;
    }
}

static bool mmay_mir_range_optimizer_invalidates(
    MarimayMirInstructionKind kind
) {
    return kind == MMAY_MIR_CALL || kind == MMAY_MIR_CALL_INTERFACE
        || kind == MMAY_MIR_ARRAY_SET || kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
        || kind == MMAY_MIR_ARRAY_PUSH
        || kind == MMAY_MIR_ARRAY_POP || kind == MMAY_MIR_ARRAY_REMOVE_AT
        || kind == MMAY_MIR_ARRAY_TRUNCATE
        || kind == MMAY_MIR_ARRAY_CLEAR || kind == MMAY_MIR_ARRAY_RESERVE;
}

static MarimayMirRangeOptimizeStatus mmay_mir_range_status(
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

static bool mmay_mir_range_has_candidate(const MarimayMirFunction *function) {
    size_t block_index = 0;
    if (function == NULL) {
        return false;
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
            if (instruction == NULL || (instruction->kind != MMAY_MIR_ARRAY_GET
                    && instruction->kind != MMAY_MIR_ARRAY_SET)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

static MarimayMirRangeOptimizeStatus mmay_mir_optimize_array_ranges(
    MarimayMirFunction *function
) {
    MarimayMirRangeFact *facts = NULL;
    MarimayMirRangeRewrite *rewrites = NULL;
    size_t register_count = 0, rewrite_count = 0, fact_bytes = 0;
    size_t rewrite_bytes = 0, block_index = 0;
    size_t analysis_work = 0;
    MarimayMirVerifyStatus verify_status;
    MarimayMirRangeAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    if (function == NULL || !function->initialized) {
        return MMAY_MIR_RANGE_OPTIMIZE_INVALID_ARGUMENT;
    }
    if (!mmay_mir_range_has_candidate(function)) {
        return MMAY_MIR_RANGE_OPTIMIZE_OK;
    }
    verify_status = mmay_mir_verify(function);
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        return mmay_mir_range_status(verify_status);
    }
    register_count = function->register_types.length;
    if (!mmay_size_multiply(
            register_count == 0 ? 1 : register_count, sizeof(*facts),
            &fact_bytes
        ) || !mmay_size_multiply(
            function->instruction_count == 0 ? 1 : function->instruction_count,
            sizeof(*rewrites), &rewrite_bytes
        )) {
        return MMAY_MIR_RANGE_OPTIMIZE_LIMIT_REACHED;
    }
    facts = mmay_allocate_array(
        &function->allocator, register_count == 0 ? 1 : register_count,
        sizeof(*facts)
    );
    rewrites = mmay_allocate_array(
        &function->allocator,
        function->instruction_count == 0 ? 1 : function->instruction_count,
        sizeof(*rewrites)
    );
    if (facts == NULL || rewrites == NULL) {
        mmay_deallocate(&function->allocator, facts, fact_bytes);
        mmay_deallocate(&function->allocator, rewrites, rewrite_bytes);
        return MMAY_MIR_RANGE_OPTIMIZE_ALLOCATION_FAILED;
    }
    verify_status = mmay_mir_range_analysis_init(
        function, &analysis_work, &analysis
    );
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        mmay_deallocate(&function->allocator, facts, fact_bytes);
        mmay_deallocate(&function->allocator, rewrites, rewrite_bytes);
        return mmay_mir_range_status(verify_status);
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, block_index);
        size_t instruction_index = 0;
        mmay_mir_range_optimizer_clear(facts, register_count);
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if ((instruction->kind == MMAY_MIR_ARRAY_GET
                    || instruction->kind == MMAY_MIR_ARRAY_SET)
                && facts[instruction->first].index == instruction->second) {
                rewrites[rewrite_count].instruction = instruction;
                rewrites[rewrite_count].original_kind = instruction->kind;
                rewrite_count += 1;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                && mmay_mir_range_assumption_valid(
                    &analysis, function, (MarimayMirBlockId)block_index,
                    instruction_index, instruction
                )) {
                rewrites[rewrite_count].instruction = instruction;
                rewrites[rewrite_count].original_kind = instruction->kind;
                rewrite_count += 1;
            }
            if (mmay_mir_range_optimizer_invalidates(instruction->kind)) {
                mmay_mir_range_optimizer_clear(facts, register_count);
                continue;
            }
            if (instruction->target < register_count) {
                size_t fact_index = 0;
                for (fact_index = 0; fact_index < register_count;
                    fact_index += 1) {
                    if (facts[fact_index].index == instruction->target) {
                        facts[fact_index].index = MMAY_MIR_NO_REGISTER;
                    }
                }
                facts[instruction->target].index =
                    instruction->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                        ? instruction->second : MMAY_MIR_NO_REGISTER;
            }
        }
    }
    for (block_index = 0; block_index < rewrite_count; block_index += 1) {
        switch (rewrites[block_index].original_kind) {
            case MMAY_MIR_ARRAY_REQUIRE_INDEX:
                rewrites[block_index].instruction->kind =
                    MMAY_MIR_ARRAY_ASSUME_INDEX;
                break;
            case MMAY_MIR_ARRAY_GET:
                rewrites[block_index].instruction->kind =
                    MMAY_MIR_ARRAY_GET_IN_BOUNDS;
                break;
            case MMAY_MIR_ARRAY_SET:
                rewrites[block_index].instruction->kind =
                    MMAY_MIR_ARRAY_SET_IN_BOUNDS;
                break;
            default:
                verify_status = MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                break;
        }
        if (verify_status != MMAY_MIR_VERIFY_OK) {
            break;
        }
    }
    if (verify_status == MMAY_MIR_VERIFY_OK) {
        verify_status = mmay_mir_verify(function);
    }
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        for (block_index = 0; block_index < rewrite_count; block_index += 1) {
            rewrites[block_index].instruction->kind =
                rewrites[block_index].original_kind;
        }
    }
    mmay_mir_range_analysis_dispose(&analysis);
    mmay_deallocate(&function->allocator, facts, fact_bytes);
    mmay_deallocate(&function->allocator, rewrites, rewrite_bytes);
    return mmay_mir_range_status(verify_status);
}

MarimayMirRangeOptimizeStatus mmay_mir_optimize_ranges(
    MarimayMirFunction *function
) {
    MarimayMirRangeOptimizeStatus status = mmay_mir_optimize_array_ranges(
        function
    );
    return status == MMAY_MIR_RANGE_OPTIMIZE_OK
        ? mmay_mir_optimize_integer_ranges(function) : status;
}
