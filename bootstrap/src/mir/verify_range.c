#include "mir_internal.h"

#include <string.h>

typedef struct MarimayMirRangeProof {
    MarimayMirRegister array;
    MarimayMirRegister index;
} MarimayMirRangeProof;

static void mmay_mir_range_clear(
    MarimayMirRangeProof *proofs,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        proofs[index].array = MMAY_MIR_NO_REGISTER;
        proofs[index].index = MMAY_MIR_NO_REGISTER;
    }
}

static bool mmay_mir_range_invalidates(
    MarimayMirInstructionKind kind
) {
    return kind == MMAY_MIR_CALL || kind == MMAY_MIR_CALL_INTERFACE
        || kind == MMAY_MIR_ARRAY_SET || kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
        || kind == MMAY_MIR_ARRAY_PUSH || kind == MMAY_MIR_ARRAY_POP
        || kind == MMAY_MIR_ARRAY_TRUNCATE || kind == MMAY_MIR_ARRAY_CLEAR
        || kind == MMAY_MIR_ARRAY_RESERVE;
}

static bool mmay_mir_range_has_edge(
    const MarimayMirTerminator *terminator,
    MarimayMirBlockId target
) {
    return terminator != NULL
        && ((terminator->kind == MMAY_MIR_GOTO
                && terminator->target == target)
            || (terminator->kind == MMAY_MIR_BRANCH
                && (terminator->target == target
                    || terminator->otherwise == target)));
}

static bool mmay_mir_range_nonnegative_transfer(
    const MarimayMirBlock *block,
    unsigned char *facts,
    size_t register_count,
    size_t *work,
    size_t work_limit
) {
    size_t instruction_index = 0;
    if (block == NULL || facts == NULL || work == NULL) {
        return false;
    }
    for (instruction_index = 0;
        instruction_index < block->instructions.length;
        instruction_index += 1) {
        const MarimayMirInstruction *instruction = mmay_vector_at_const(
            &block->instructions, instruction_index
        );
        bool nonnegative = false;
        if (!mmay_mir_consume_verify_work(work, 1, work_limit)
            || instruction == NULL || instruction->target >= register_count) {
            return false;
        }
        switch (instruction->kind) {
            case MMAY_MIR_CONSTANT_I64:
                nonnegative = instruction->integer >= 0;
                break;
            case MMAY_MIR_COPY:
                nonnegative = instruction->first < register_count
                    && facts[instruction->first] != 0;
                break;
            case MMAY_MIR_ADD_CHECKED_I64:
            case MMAY_MIR_ADD_PROVEN_I64:
                nonnegative = instruction->first < register_count
                    && instruction->second < register_count
                    && facts[instruction->first] != 0
                    && facts[instruction->second] != 0;
                break;
            case MMAY_MIR_ARRAY_LENGTH:
                nonnegative = true;
                break;
            default:
                break;
        }
        facts[instruction->target] = nonnegative ? 1 : 0;
    }
    return true;
}

static MarimayMirVerifyStatus mmay_mir_range_mark_reachable(
    const MarimayMirFunction *function,
    unsigned char *reachable,
    size_t *work
) {
    bool changed = true;
    size_t block_index = 0;
    if (function == NULL || reachable == NULL || work == NULL) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    reachable[0] = 1;
    while (changed) {
        changed = false;
        for (block_index = 0; block_index < function->blocks.length;
            block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(
                &function->blocks, block_index
            );
            const MarimayMirTerminator *terminator = block != NULL
                ? &block->terminator : NULL;
            MarimayMirBlockId targets[2];
            size_t target_count = 0, target_index = 0;
            if (!mmay_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                )) {
                return MMAY_MIR_VERIFY_LIMIT_REACHED;
            }
            if (block == NULL || terminator == NULL) {
                return MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
            }
            if (reachable[block_index] == 0) {
                continue;
            }
            if (terminator->kind == MMAY_MIR_GOTO) {
                targets[target_count] = terminator->target;
                target_count += 1;
            } else if (terminator->kind == MMAY_MIR_BRANCH) {
                targets[target_count] = terminator->target;
                target_count += 1;
                targets[target_count] = terminator->otherwise;
                target_count += 1;
            }
            for (target_index = 0; target_index < target_count;
                target_index += 1) {
                if (targets[target_index] >= function->blocks.length) {
                    return MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
                }
                if (reachable[targets[target_index]] == 0) {
                    reachable[targets[target_index]] = 1;
                    changed = true;
                }
            }
        }
    }
    return MMAY_MIR_VERIFY_OK;
}

MarimayMirVerifyStatus mmay_mir_range_analysis_init(
    const MarimayMirFunction *function,
    size_t *work,
    MarimayMirRangeAnalysis *analysis
) {
    unsigned char *outgoing = NULL, *reachable = NULL;
    size_t block_count = 0, register_count = 0, cells = 0;
    size_t cell_bytes = 0, block_bytes = 0;
    size_t block_index = 0;
    bool changed = true;
    MarimayMirVerifyStatus status = MMAY_MIR_VERIFY_OK;
    if (analysis != NULL) {
        memset(analysis, 0, sizeof(*analysis));
    }
    if (function == NULL || work == NULL || analysis == NULL
        || function->blocks.length == 0) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    block_count = function->blocks.length;
    register_count = function->register_types.length;
    if (!mmay_size_multiply(block_count, register_count, &cells)
        || cells > MMAY_MIR_MAX_DATAFLOW_CELLS) {
        return MMAY_MIR_VERIFY_LIMIT_REACHED;
    }
    cell_bytes = cells == 0 ? 1 : cells;
    block_bytes = block_count == 0 ? 1 : block_count;
    analysis->function = function;
    analysis->incoming_bytes = cell_bytes;
    analysis->scratch_bytes = register_count == 0 ? 1 : register_count;
    analysis->incoming_nonnegative = mmay_allocate_array(
        &function->allocator, cell_bytes, sizeof(unsigned char)
    );
    outgoing = mmay_allocate_array(
        &function->allocator, cell_bytes, sizeof(unsigned char)
    );
    analysis->scratch = mmay_allocate_array(
        &function->allocator, register_count == 0 ? 1 : register_count,
        sizeof(unsigned char)
    );
    reachable = mmay_allocate_array(
        &function->allocator, block_bytes, sizeof(unsigned char)
    );
    if (analysis->incoming_nonnegative == NULL || outgoing == NULL
        || analysis->scratch == NULL || reachable == NULL) {
        status = MMAY_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(analysis->incoming_nonnegative, 1, cell_bytes);
    memset(outgoing, 1, cell_bytes);
    memset(reachable, 0, block_bytes);
    status = mmay_mir_range_mark_reachable(function, reachable, work);
    if (status != MMAY_MIR_VERIFY_OK) {
        goto cleanup;
    }
    if (register_count != 0) {
        memset(analysis->incoming_nonnegative, 0, register_count);
    }
    while (changed) {
        changed = false;
        for (block_index = 0; block_index < block_count; block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(
                &function->blocks, block_index
            );
            unsigned char *incoming = analysis->incoming_nonnegative
                + block_index * register_count;
            unsigned char *outgoing_row = outgoing
                + block_index * register_count;
            size_t predecessor = 0, register_index = 0;
            bool has_predecessor = false;
            if (!mmay_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                ) || block == NULL) {
                status = block == NULL
                    ? MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW
                    : MMAY_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (reachable[block_index] == 0) {
                continue;
            }
            if (block_index == 0) {
                memset(analysis->scratch, 0, register_count);
            } else {
                memset(analysis->scratch, 1, register_count);
                for (predecessor = 0; predecessor < block_count;
                    predecessor += 1) {
                    const MarimayMirBlock *candidate = mmay_vector_at_const(
                        &function->blocks, predecessor
                    );
                    const unsigned char *candidate_outgoing = outgoing
                        + predecessor * register_count;
                    if (!mmay_mir_consume_verify_work(
                            work, 1, function->options.max_verify_work
                        )) {
                        status = MMAY_MIR_VERIFY_LIMIT_REACHED;
                        goto cleanup;
                    }
                    if (reachable[predecessor] == 0 || candidate == NULL
                        || !mmay_mir_range_has_edge(
                            &candidate->terminator,
                            (MarimayMirBlockId)block_index
                        )) {
                        continue;
                    }
                    has_predecessor = true;
                    for (register_index = 0;
                        register_index < register_count;
                        register_index += 1) {
                        analysis->scratch[register_index] =
                            analysis->scratch[register_index] != 0
                                && candidate_outgoing[register_index] != 0;
                    }
                }
                if (!has_predecessor) {
                    memset(analysis->scratch, 0, register_count);
                }
            }
            if (memcmp(incoming, analysis->scratch, register_count) != 0) {
                memcpy(incoming, analysis->scratch, register_count);
                changed = true;
            }
            memcpy(analysis->scratch, incoming, register_count);
            if (!mmay_mir_range_nonnegative_transfer(
                    block, analysis->scratch, register_count, work,
                    function->options.max_verify_work
                )) {
                status = *work >= function->options.max_verify_work
                    ? MMAY_MIR_VERIFY_LIMIT_REACHED
                    : MMAY_MIR_VERIFY_INVALID_REGISTER;
                goto cleanup;
            }
            if (memcmp(outgoing_row, analysis->scratch, register_count) != 0) {
                memcpy(outgoing_row, analysis->scratch, register_count);
                changed = true;
            }
        }
    }
cleanup:
    mmay_deallocate(&function->allocator, outgoing, cell_bytes);
    mmay_deallocate(&function->allocator, reachable, block_bytes);
    if (status != MMAY_MIR_VERIFY_OK) {
        mmay_mir_range_analysis_dispose(analysis);
    }
    return status;
}

void mmay_mir_range_analysis_dispose(MarimayMirRangeAnalysis *analysis) {
    if (analysis == NULL) {
        return;
    }
    if (analysis->function != NULL) {
        mmay_deallocate(
            &analysis->function->allocator, analysis->incoming_nonnegative,
            analysis->incoming_bytes
        );
        mmay_deallocate(
            &analysis->function->allocator, analysis->scratch,
            analysis->scratch_bytes
        );
    }
    memset(analysis, 0, sizeof(*analysis));
}

static bool mmay_mir_range_nonnegative_at(
    MarimayMirRangeAnalysis *analysis,
    MarimayMirBlockId block_id,
    size_t instruction_index,
    MarimayMirRegister register_id
) {
    const MarimayMirBlock *block = NULL;
    size_t register_count = 0;
    size_t cursor = 0;
    if (analysis == NULL || analysis->function == NULL
        || block_id >= analysis->function->blocks.length) {
        return false;
    }
    block = mmay_vector_at_const(&analysis->function->blocks, block_id);
    register_count = analysis->function->register_types.length;
    if (block == NULL || instruction_index > block->instructions.length
        || register_id >= register_count) {
        return false;
    }
    memcpy(
        analysis->scratch,
        analysis->incoming_nonnegative + block_id * register_count,
        register_count
    );
    for (cursor = 0; cursor < instruction_index; cursor += 1) {
        const MarimayMirInstruction *instruction = mmay_vector_at_const(
            &block->instructions, cursor
        );
        if (instruction == NULL || instruction->target >= register_count) {
            return false;
        }
        switch (instruction->kind) {
            case MMAY_MIR_CONSTANT_I64:
                analysis->scratch[instruction->target] =
                    instruction->integer >= 0;
                break;
            case MMAY_MIR_COPY:
                analysis->scratch[instruction->target] =
                    instruction->first < register_count
                        && analysis->scratch[instruction->first] != 0;
                break;
            case MMAY_MIR_ADD_CHECKED_I64:
            case MMAY_MIR_ADD_PROVEN_I64:
                analysis->scratch[instruction->target] =
                    instruction->first < register_count
                        && instruction->second < register_count
                        && analysis->scratch[instruction->first] != 0
                        && analysis->scratch[instruction->second] != 0;
                break;
            case MMAY_MIR_ARRAY_LENGTH:
                analysis->scratch[instruction->target] = 1;
                break;
            default:
                analysis->scratch[instruction->target] = 0;
                break;
        }
    }
    return analysis->scratch[register_id] != 0;
}

static bool mmay_mir_range_prefix_preserves(
    const MarimayMirBlock *block,
    size_t start,
    size_t end,
    MarimayMirRegister array,
    MarimayMirRegister index,
    MarimayMirRegister length
) {
    size_t cursor = 0;
    if (block == NULL || start > end || end > block->instructions.length) {
        return false;
    }
    for (cursor = start; cursor < end; cursor += 1) {
        const MarimayMirInstruction *current = mmay_vector_at_const(
            &block->instructions, cursor
        );
        if (current == NULL || mmay_mir_range_invalidates(current->kind)
            || (array != MMAY_MIR_NO_REGISTER && current->target == array)
            || (index != MMAY_MIR_NO_REGISTER && current->target == index)
            || (length != MMAY_MIR_NO_REGISTER && current->target == length)) {
            return false;
        }
    }
    return true;
}

static const MarimayMirInstruction *mmay_mir_range_last_definition(
    const MarimayMirBlock *block,
    MarimayMirRegister target,
    size_t end,
    size_t *definition_index
) {
    size_t cursor = 0;
    if (definition_index != NULL) {
        *definition_index = SIZE_MAX;
    }
    if (block == NULL || end > block->instructions.length) {
        return NULL;
    }
    cursor = end;
    while (cursor > 0) {
        const MarimayMirInstruction *current = NULL;
        cursor -= 1;
        current = mmay_vector_at_const(&block->instructions, cursor);
        if (current != NULL && current->target == target) {
            if (definition_index != NULL) {
                *definition_index = cursor;
            }
            return current;
        }
    }
    return NULL;
}

static bool mmay_mir_range_literal_assumption_valid(
    const MarimayMirFunction *function,
    const MarimayMirBlock *block,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirInstruction *array_new = NULL;
    const MarimayMirInstruction *constant = NULL;
    const MarimayMirArrayConstruction *construction = NULL;
    size_t array_new_index = SIZE_MAX;
    size_t constant_index = SIZE_MAX;
    if (function == NULL || block == NULL || instruction == NULL) {
        return false;
    }
    array_new = mmay_mir_range_last_definition(
        block, instruction->first, instruction_index, &array_new_index
    );
    constant = mmay_mir_range_last_definition(
        block, instruction->second, instruction_index, &constant_index
    );
    if (array_new == NULL || constant == NULL
        || array_new->kind != MMAY_MIR_ARRAY_NEW
        || array_new->array != instruction->array
        || constant->kind != MMAY_MIR_CONSTANT_I64
        || constant->integer < 0
        || array_new->array_construction >= function->array_constructions.length) {
        return false;
    }
    construction = mmay_vector_at_const(
        &function->array_constructions, array_new->array_construction
    );
    if (construction == NULL
        || (uint64_t)constant->integer >= construction->argument_count) {
        return false;
    }
    return mmay_mir_range_prefix_preserves(
        block, array_new_index + 1, instruction_index, instruction->first,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER
    ) && mmay_mir_range_prefix_preserves(
        block, constant_index + 1, instruction_index,
        MMAY_MIR_NO_REGISTER, instruction->second, MMAY_MIR_NO_REGISTER
    );
}

static bool mmay_mir_range_branch_assumption_valid(
    MarimayMirRangeAnalysis *analysis,
    const MarimayMirFunction *function,
    MarimayMirBlockId block_id,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
) {
    size_t predecessor_id = 0;
    bool saw_predecessor = false;
    const MarimayMirBlock *successor = mmay_vector_at_const(
        &function->blocks, block_id
    );
    if (analysis == NULL || successor == NULL
        || !mmay_mir_range_nonnegative_at(
            analysis, block_id, instruction_index, instruction->second
        ) || !mmay_mir_range_prefix_preserves(
            successor, 0, instruction_index, instruction->first,
            instruction->second, MMAY_MIR_NO_REGISTER
        )) {
        return false;
    }
    for (predecessor_id = 0;
        predecessor_id < function->blocks.length;
        predecessor_id += 1) {
        const MarimayMirBlock *predecessor = mmay_vector_at_const(
            &function->blocks, predecessor_id
        );
        const MarimayMirInstruction *condition = NULL;
        const MarimayMirInstruction *length = NULL;
        size_t condition_index = SIZE_MAX;
        size_t length_index = SIZE_MAX;
        if (predecessor == NULL || (predecessor->terminator.target != block_id
            && predecessor->terminator.otherwise != block_id)) {
            continue;
        }
        saw_predecessor = true;
        if (predecessor->terminator.kind != MMAY_MIR_BRANCH
            || predecessor->terminator.target != block_id) {
            return false;
        }
        condition = mmay_mir_range_last_definition(
            predecessor, predecessor->terminator.value,
            predecessor->instructions.length, &condition_index
        );
        if (condition == NULL || condition->kind != MMAY_MIR_LESS_I64
            || condition->first != instruction->second) {
            return false;
        }
        length = mmay_mir_range_last_definition(
            predecessor, condition->second, condition_index, &length_index
        );
        if (length == NULL || length->kind != MMAY_MIR_ARRAY_LENGTH
            || length->first != instruction->first
            || !mmay_mir_range_prefix_preserves(
                predecessor, length_index + 1,
                predecessor->instructions.length, instruction->first,
                instruction->second, condition->second
            )) {
            return false;
        }
    }
    return saw_predecessor;
}

bool mmay_mir_range_assumption_valid(
    MarimayMirRangeAnalysis *analysis,
    const MarimayMirFunction *function,
    MarimayMirBlockId block_id,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
) {
    if (function == NULL || instruction == NULL
        || (instruction->kind != MMAY_MIR_ARRAY_REQUIRE_INDEX
            && instruction->kind != MMAY_MIR_ARRAY_ASSUME_INDEX)
        || block_id >= function->blocks.length) {
        return false;
    }
    return mmay_mir_range_literal_assumption_valid(
        function, mmay_vector_at_const(&function->blocks, block_id),
        instruction_index, instruction
    ) || mmay_mir_range_branch_assumption_valid(
        analysis, function, block_id, instruction_index, instruction
    );
}

static void mmay_mir_range_advance(
    const MarimayMirInstruction *instruction,
    MarimayMirRangeProof *proofs,
    size_t register_count
) {
    size_t index = 0;
    if (instruction == NULL || proofs == NULL) {
        return;
    }
    if (mmay_mir_range_invalidates(instruction->kind)) {
        mmay_mir_range_clear(proofs, register_count);
        return;
    }
    if (instruction->target >= register_count) {
        return;
    }
    for (index = 0; index < register_count; index += 1) {
        if (proofs[index].index == instruction->target
            || proofs[index].array == instruction->target) {
            proofs[index].array = MMAY_MIR_NO_REGISTER;
            proofs[index].index = MMAY_MIR_NO_REGISTER;
        }
    }
    if (instruction->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
        || instruction->kind == MMAY_MIR_ARRAY_ASSUME_INDEX) {
        proofs[instruction->target].array = instruction->first;
        proofs[instruction->target].index = instruction->second;
    } else {
        proofs[instruction->target].array = MMAY_MIR_NO_REGISTER;
        proofs[instruction->target].index = MMAY_MIR_NO_REGISTER;
    }
}

MarimayMirVerifyStatus mmay_mir_verify_range_proofs(
    const MarimayMirFunction *function,
    size_t *work
) {
    MarimayMirRangeProof *proofs = NULL;
    MarimayMirRangeAnalysis analysis;
    size_t block_index = 0;
    size_t register_count = 0;
    size_t bytes = 0;
    bool has_in_bounds = false, has_assumption = false;
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
        if (block == NULL) {
            return MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && (instruction->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS
                    || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS)) {
                has_in_bounds = true;
            }
            has_assumption = has_assumption || (instruction != NULL
                && instruction->kind == MMAY_MIR_ARRAY_ASSUME_INDEX);
        }
    }
    if (!has_in_bounds && !has_assumption) {
        return MMAY_MIR_VERIFY_OK;
    }
    register_count = function->register_types.length;
    if (!mmay_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(*proofs),
            &bytes
        )) {
        return MMAY_MIR_VERIFY_LIMIT_REACHED;
    }
    proofs = mmay_allocate_array(
        &function->allocator,
        register_count == 0 ? 1 : register_count,
        sizeof(*proofs)
    );
    if (proofs == NULL) {
        return MMAY_MIR_VERIFY_ALLOCATION_FAILED;
    }
    status = mmay_mir_range_analysis_init(function, work, &analysis);
    if (status != MMAY_MIR_VERIFY_OK) {
        mmay_deallocate(&function->allocator, proofs, bytes);
        return status;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        mmay_mir_range_clear(proofs, register_count);
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (!mmay_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                )) {
                status = MMAY_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (instruction == NULL || instruction->target >= register_count) {
                status = MMAY_MIR_VERIFY_INVALID_REGISTER;
                goto cleanup;
            }
            if (instruction->kind == MMAY_MIR_ARRAY_ASSUME_INDEX
                && !mmay_mir_range_assumption_valid(
                    &analysis, function, (MarimayMirBlockId)block_index,
                    instruction_index, instruction
                )) {
                status = MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if ((instruction->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS
                    || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS)
                && (instruction->first >= register_count
                    || instruction->second >= register_count
                    || proofs[instruction->first].index
                        != instruction->second)) {
                status = MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            mmay_mir_range_advance(instruction, proofs, register_count);
        }
    }
cleanup:
    mmay_mir_range_analysis_dispose(&analysis);
    mmay_deallocate(&function->allocator, proofs, bytes);
    return status;
}
