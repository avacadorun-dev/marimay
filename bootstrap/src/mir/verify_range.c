#include "mir_internal.h"

#include <string.h>

typedef struct RudaMirRangeProof {
    RudaMirRegister array;
    RudaMirRegister index;
} RudaMirRangeProof;

static void ruda_mir_range_clear(
    RudaMirRangeProof *proofs,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        proofs[index].array = RUDA_MIR_NO_REGISTER;
        proofs[index].index = RUDA_MIR_NO_REGISTER;
    }
}

static bool ruda_mir_range_invalidates(
    RudaMirInstructionKind kind
) {
    return kind == RUDA_MIR_CALL || kind == RUDA_MIR_CALL_INTERFACE
        || kind == RUDA_MIR_ARRAY_SET || kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS
        || kind == RUDA_MIR_ARRAY_PUSH || kind == RUDA_MIR_ARRAY_POP
        || kind == RUDA_MIR_ARRAY_TRUNCATE || kind == RUDA_MIR_ARRAY_CLEAR
        || kind == RUDA_MIR_ARRAY_RESERVE;
}

static bool ruda_mir_range_has_edge(
    const RudaMirTerminator *terminator,
    RudaMirBlockId target
) {
    return terminator != NULL
        && ((terminator->kind == RUDA_MIR_GOTO
                && terminator->target == target)
            || (terminator->kind == RUDA_MIR_BRANCH
                && (terminator->target == target
                    || terminator->otherwise == target)));
}

static bool ruda_mir_range_nonnegative_transfer(
    const RudaMirBlock *block,
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
        const RudaMirInstruction *instruction = ruda_vector_at_const(
            &block->instructions, instruction_index
        );
        bool nonnegative = false;
        if (!ruda_mir_consume_verify_work(work, 1, work_limit)
            || instruction == NULL || instruction->target >= register_count) {
            return false;
        }
        switch (instruction->kind) {
            case RUDA_MIR_CONSTANT_I64:
                nonnegative = instruction->integer >= 0;
                break;
            case RUDA_MIR_COPY:
                nonnegative = instruction->first < register_count
                    && facts[instruction->first] != 0;
                break;
            case RUDA_MIR_ADD_CHECKED_I64:
            case RUDA_MIR_ADD_PROVEN_I64:
                nonnegative = instruction->first < register_count
                    && instruction->second < register_count
                    && facts[instruction->first] != 0
                    && facts[instruction->second] != 0;
                break;
            case RUDA_MIR_ARRAY_LENGTH:
                nonnegative = true;
                break;
            default:
                break;
        }
        facts[instruction->target] = nonnegative ? 1 : 0;
    }
    return true;
}

static RudaMirVerifyStatus ruda_mir_range_mark_reachable(
    const RudaMirFunction *function,
    unsigned char *reachable,
    size_t *work
) {
    bool changed = true;
    size_t block_index = 0;
    if (function == NULL || reachable == NULL || work == NULL) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    reachable[0] = 1;
    while (changed) {
        changed = false;
        for (block_index = 0; block_index < function->blocks.length;
            block_index += 1) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->blocks, block_index
            );
            const RudaMirTerminator *terminator = block != NULL
                ? &block->terminator : NULL;
            RudaMirBlockId targets[2];
            size_t target_count = 0, target_index = 0;
            if (!ruda_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                )) {
                return RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
            if (block == NULL || terminator == NULL) {
                return RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            }
            if (reachable[block_index] == 0) {
                continue;
            }
            if (terminator->kind == RUDA_MIR_GOTO) {
                targets[target_count] = terminator->target;
                target_count += 1;
            } else if (terminator->kind == RUDA_MIR_BRANCH) {
                targets[target_count] = terminator->target;
                target_count += 1;
                targets[target_count] = terminator->otherwise;
                target_count += 1;
            }
            for (target_index = 0; target_index < target_count;
                target_index += 1) {
                if (targets[target_index] >= function->blocks.length) {
                    return RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                }
                if (reachable[targets[target_index]] == 0) {
                    reachable[targets[target_index]] = 1;
                    changed = true;
                }
            }
        }
    }
    return RUDA_MIR_VERIFY_OK;
}

RudaMirVerifyStatus ruda_mir_range_analysis_init(
    const RudaMirFunction *function,
    size_t *work,
    RudaMirRangeAnalysis *analysis
) {
    unsigned char *outgoing = NULL, *reachable = NULL;
    size_t block_count = 0, register_count = 0, cells = 0;
    size_t cell_bytes = 0, block_bytes = 0;
    size_t block_index = 0;
    bool changed = true;
    RudaMirVerifyStatus status = RUDA_MIR_VERIFY_OK;
    if (analysis != NULL) {
        memset(analysis, 0, sizeof(*analysis));
    }
    if (function == NULL || work == NULL || analysis == NULL
        || function->blocks.length == 0) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    block_count = function->blocks.length;
    register_count = function->register_types.length;
    if (!ruda_size_multiply(block_count, register_count, &cells)
        || cells > RUDA_MIR_MAX_DATAFLOW_CELLS) {
        return RUDA_MIR_VERIFY_LIMIT_REACHED;
    }
    cell_bytes = cells == 0 ? 1 : cells;
    block_bytes = block_count == 0 ? 1 : block_count;
    analysis->function = function;
    analysis->incoming_bytes = cell_bytes;
    analysis->scratch_bytes = register_count == 0 ? 1 : register_count;
    analysis->incoming_nonnegative = ruda_allocate_array(
        &function->allocator, cell_bytes, sizeof(unsigned char)
    );
    outgoing = ruda_allocate_array(
        &function->allocator, cell_bytes, sizeof(unsigned char)
    );
    analysis->scratch = ruda_allocate_array(
        &function->allocator, register_count == 0 ? 1 : register_count,
        sizeof(unsigned char)
    );
    reachable = ruda_allocate_array(
        &function->allocator, block_bytes, sizeof(unsigned char)
    );
    if (analysis->incoming_nonnegative == NULL || outgoing == NULL
        || analysis->scratch == NULL || reachable == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(analysis->incoming_nonnegative, 1, cell_bytes);
    memset(outgoing, 1, cell_bytes);
    memset(reachable, 0, block_bytes);
    status = ruda_mir_range_mark_reachable(function, reachable, work);
    if (status != RUDA_MIR_VERIFY_OK) {
        goto cleanup;
    }
    if (register_count != 0) {
        memset(analysis->incoming_nonnegative, 0, register_count);
    }
    while (changed) {
        changed = false;
        for (block_index = 0; block_index < block_count; block_index += 1) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->blocks, block_index
            );
            unsigned char *incoming = analysis->incoming_nonnegative
                + block_index * register_count;
            unsigned char *outgoing_row = outgoing
                + block_index * register_count;
            size_t predecessor = 0, register_index = 0;
            bool has_predecessor = false;
            if (!ruda_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                ) || block == NULL) {
                status = block == NULL
                    ? RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW
                    : RUDA_MIR_VERIFY_LIMIT_REACHED;
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
                    const RudaMirBlock *candidate = ruda_vector_at_const(
                        &function->blocks, predecessor
                    );
                    const unsigned char *candidate_outgoing = outgoing
                        + predecessor * register_count;
                    if (!ruda_mir_consume_verify_work(
                            work, 1, function->options.max_verify_work
                        )) {
                        status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                        goto cleanup;
                    }
                    if (reachable[predecessor] == 0 || candidate == NULL
                        || !ruda_mir_range_has_edge(
                            &candidate->terminator,
                            (RudaMirBlockId)block_index
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
            if (!ruda_mir_range_nonnegative_transfer(
                    block, analysis->scratch, register_count, work,
                    function->options.max_verify_work
                )) {
                status = *work >= function->options.max_verify_work
                    ? RUDA_MIR_VERIFY_LIMIT_REACHED
                    : RUDA_MIR_VERIFY_INVALID_REGISTER;
                goto cleanup;
            }
            if (memcmp(outgoing_row, analysis->scratch, register_count) != 0) {
                memcpy(outgoing_row, analysis->scratch, register_count);
                changed = true;
            }
        }
    }
cleanup:
    ruda_deallocate(&function->allocator, outgoing, cell_bytes);
    ruda_deallocate(&function->allocator, reachable, block_bytes);
    if (status != RUDA_MIR_VERIFY_OK) {
        ruda_mir_range_analysis_dispose(analysis);
    }
    return status;
}

void ruda_mir_range_analysis_dispose(RudaMirRangeAnalysis *analysis) {
    if (analysis == NULL) {
        return;
    }
    if (analysis->function != NULL) {
        ruda_deallocate(
            &analysis->function->allocator, analysis->incoming_nonnegative,
            analysis->incoming_bytes
        );
        ruda_deallocate(
            &analysis->function->allocator, analysis->scratch,
            analysis->scratch_bytes
        );
    }
    memset(analysis, 0, sizeof(*analysis));
}

static bool ruda_mir_range_nonnegative_at(
    RudaMirRangeAnalysis *analysis,
    RudaMirBlockId block_id,
    size_t instruction_index,
    RudaMirRegister register_id
) {
    const RudaMirBlock *block = NULL;
    size_t register_count = 0;
    size_t cursor = 0;
    if (analysis == NULL || analysis->function == NULL
        || block_id >= analysis->function->blocks.length) {
        return false;
    }
    block = ruda_vector_at_const(&analysis->function->blocks, block_id);
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
        const RudaMirInstruction *instruction = ruda_vector_at_const(
            &block->instructions, cursor
        );
        if (instruction == NULL || instruction->target >= register_count) {
            return false;
        }
        switch (instruction->kind) {
            case RUDA_MIR_CONSTANT_I64:
                analysis->scratch[instruction->target] =
                    instruction->integer >= 0;
                break;
            case RUDA_MIR_COPY:
                analysis->scratch[instruction->target] =
                    instruction->first < register_count
                        && analysis->scratch[instruction->first] != 0;
                break;
            case RUDA_MIR_ADD_CHECKED_I64:
            case RUDA_MIR_ADD_PROVEN_I64:
                analysis->scratch[instruction->target] =
                    instruction->first < register_count
                        && instruction->second < register_count
                        && analysis->scratch[instruction->first] != 0
                        && analysis->scratch[instruction->second] != 0;
                break;
            case RUDA_MIR_ARRAY_LENGTH:
                analysis->scratch[instruction->target] = 1;
                break;
            default:
                analysis->scratch[instruction->target] = 0;
                break;
        }
    }
    return analysis->scratch[register_id] != 0;
}

static bool ruda_mir_range_prefix_preserves(
    const RudaMirBlock *block,
    size_t start,
    size_t end,
    RudaMirRegister array,
    RudaMirRegister index,
    RudaMirRegister length
) {
    size_t cursor = 0;
    if (block == NULL || start > end || end > block->instructions.length) {
        return false;
    }
    for (cursor = start; cursor < end; cursor += 1) {
        const RudaMirInstruction *current = ruda_vector_at_const(
            &block->instructions, cursor
        );
        if (current == NULL || ruda_mir_range_invalidates(current->kind)
            || (array != RUDA_MIR_NO_REGISTER && current->target == array)
            || (index != RUDA_MIR_NO_REGISTER && current->target == index)
            || (length != RUDA_MIR_NO_REGISTER && current->target == length)) {
            return false;
        }
    }
    return true;
}

static const RudaMirInstruction *ruda_mir_range_last_definition(
    const RudaMirBlock *block,
    RudaMirRegister target,
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
        const RudaMirInstruction *current = NULL;
        cursor -= 1;
        current = ruda_vector_at_const(&block->instructions, cursor);
        if (current != NULL && current->target == target) {
            if (definition_index != NULL) {
                *definition_index = cursor;
            }
            return current;
        }
    }
    return NULL;
}

static bool ruda_mir_range_literal_assumption_valid(
    const RudaMirFunction *function,
    const RudaMirBlock *block,
    size_t instruction_index,
    const RudaMirInstruction *instruction
) {
    const RudaMirInstruction *array_new = NULL;
    const RudaMirInstruction *constant = NULL;
    const RudaMirArrayConstruction *construction = NULL;
    size_t array_new_index = SIZE_MAX;
    size_t constant_index = SIZE_MAX;
    if (function == NULL || block == NULL || instruction == NULL) {
        return false;
    }
    array_new = ruda_mir_range_last_definition(
        block, instruction->first, instruction_index, &array_new_index
    );
    constant = ruda_mir_range_last_definition(
        block, instruction->second, instruction_index, &constant_index
    );
    if (array_new == NULL || constant == NULL
        || array_new->kind != RUDA_MIR_ARRAY_NEW
        || array_new->array != instruction->array
        || constant->kind != RUDA_MIR_CONSTANT_I64
        || constant->integer < 0
        || array_new->array_construction >= function->array_constructions.length) {
        return false;
    }
    construction = ruda_vector_at_const(
        &function->array_constructions, array_new->array_construction
    );
    if (construction == NULL
        || (uint64_t)constant->integer >= construction->argument_count) {
        return false;
    }
    return ruda_mir_range_prefix_preserves(
        block, array_new_index + 1, instruction_index, instruction->first,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER
    ) && ruda_mir_range_prefix_preserves(
        block, constant_index + 1, instruction_index,
        RUDA_MIR_NO_REGISTER, instruction->second, RUDA_MIR_NO_REGISTER
    );
}

static bool ruda_mir_range_branch_assumption_valid(
    RudaMirRangeAnalysis *analysis,
    const RudaMirFunction *function,
    RudaMirBlockId block_id,
    size_t instruction_index,
    const RudaMirInstruction *instruction
) {
    size_t predecessor_id = 0;
    bool saw_predecessor = false;
    const RudaMirBlock *successor = ruda_vector_at_const(
        &function->blocks, block_id
    );
    if (analysis == NULL || successor == NULL
        || !ruda_mir_range_nonnegative_at(
            analysis, block_id, instruction_index, instruction->second
        ) || !ruda_mir_range_prefix_preserves(
            successor, 0, instruction_index, instruction->first,
            instruction->second, RUDA_MIR_NO_REGISTER
        )) {
        return false;
    }
    for (predecessor_id = 0;
        predecessor_id < function->blocks.length;
        predecessor_id += 1) {
        const RudaMirBlock *predecessor = ruda_vector_at_const(
            &function->blocks, predecessor_id
        );
        const RudaMirInstruction *condition = NULL;
        const RudaMirInstruction *length = NULL;
        size_t condition_index = SIZE_MAX;
        size_t length_index = SIZE_MAX;
        if (predecessor == NULL || (predecessor->terminator.target != block_id
            && predecessor->terminator.otherwise != block_id)) {
            continue;
        }
        saw_predecessor = true;
        if (predecessor->terminator.kind != RUDA_MIR_BRANCH
            || predecessor->terminator.target != block_id) {
            return false;
        }
        condition = ruda_mir_range_last_definition(
            predecessor, predecessor->terminator.value,
            predecessor->instructions.length, &condition_index
        );
        if (condition == NULL || condition->kind != RUDA_MIR_LESS_I64
            || condition->first != instruction->second) {
            return false;
        }
        length = ruda_mir_range_last_definition(
            predecessor, condition->second, condition_index, &length_index
        );
        if (length == NULL || length->kind != RUDA_MIR_ARRAY_LENGTH
            || length->first != instruction->first
            || !ruda_mir_range_prefix_preserves(
                predecessor, length_index + 1,
                predecessor->instructions.length, instruction->first,
                instruction->second, condition->second
            )) {
            return false;
        }
    }
    return saw_predecessor;
}

bool ruda_mir_range_assumption_valid(
    RudaMirRangeAnalysis *analysis,
    const RudaMirFunction *function,
    RudaMirBlockId block_id,
    size_t instruction_index,
    const RudaMirInstruction *instruction
) {
    if (function == NULL || instruction == NULL
        || (instruction->kind != RUDA_MIR_ARRAY_REQUIRE_INDEX
            && instruction->kind != RUDA_MIR_ARRAY_ASSUME_INDEX)
        || block_id >= function->blocks.length) {
        return false;
    }
    return ruda_mir_range_literal_assumption_valid(
        function, ruda_vector_at_const(&function->blocks, block_id),
        instruction_index, instruction
    ) || ruda_mir_range_branch_assumption_valid(
        analysis, function, block_id, instruction_index, instruction
    );
}

static void ruda_mir_range_advance(
    const RudaMirInstruction *instruction,
    RudaMirRangeProof *proofs,
    size_t register_count
) {
    size_t index = 0;
    if (instruction == NULL || proofs == NULL) {
        return;
    }
    if (ruda_mir_range_invalidates(instruction->kind)) {
        ruda_mir_range_clear(proofs, register_count);
        return;
    }
    if (instruction->target >= register_count) {
        return;
    }
    for (index = 0; index < register_count; index += 1) {
        if (proofs[index].index == instruction->target
            || proofs[index].array == instruction->target) {
            proofs[index].array = RUDA_MIR_NO_REGISTER;
            proofs[index].index = RUDA_MIR_NO_REGISTER;
        }
    }
    if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
        || instruction->kind == RUDA_MIR_ARRAY_ASSUME_INDEX) {
        proofs[instruction->target].array = instruction->first;
        proofs[instruction->target].index = instruction->second;
    } else {
        proofs[instruction->target].array = RUDA_MIR_NO_REGISTER;
        proofs[instruction->target].index = RUDA_MIR_NO_REGISTER;
    }
}

RudaMirVerifyStatus ruda_mir_verify_range_proofs(
    const RudaMirFunction *function,
    size_t *work
) {
    RudaMirRangeProof *proofs = NULL;
    RudaMirRangeAnalysis analysis;
    size_t block_index = 0;
    size_t register_count = 0;
    size_t bytes = 0;
    bool has_in_bounds = false, has_assumption = false;
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
        if (block == NULL) {
            return RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && (instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS
                    || instruction->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS)) {
                has_in_bounds = true;
            }
            has_assumption = has_assumption || (instruction != NULL
                && instruction->kind == RUDA_MIR_ARRAY_ASSUME_INDEX);
        }
    }
    if (!has_in_bounds && !has_assumption) {
        return RUDA_MIR_VERIFY_OK;
    }
    register_count = function->register_types.length;
    if (!ruda_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(*proofs),
            &bytes
        )) {
        return RUDA_MIR_VERIFY_LIMIT_REACHED;
    }
    proofs = ruda_allocate_array(
        &function->allocator,
        register_count == 0 ? 1 : register_count,
        sizeof(*proofs)
    );
    if (proofs == NULL) {
        return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
    }
    status = ruda_mir_range_analysis_init(function, work, &analysis);
    if (status != RUDA_MIR_VERIFY_OK) {
        ruda_deallocate(&function->allocator, proofs, bytes);
        return status;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        ruda_mir_range_clear(proofs, register_count);
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (!ruda_mir_consume_verify_work(
                    work, 1, function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (instruction == NULL || instruction->target >= register_count) {
                status = RUDA_MIR_VERIFY_INVALID_REGISTER;
                goto cleanup;
            }
            if (instruction->kind == RUDA_MIR_ARRAY_ASSUME_INDEX
                && !ruda_mir_range_assumption_valid(
                    &analysis, function, (RudaMirBlockId)block_index,
                    instruction_index, instruction
                )) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if ((instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS
                    || instruction->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS)
                && (instruction->first >= register_count
                    || instruction->second >= register_count
                    || proofs[instruction->first].index
                        != instruction->second)) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            ruda_mir_range_advance(instruction, proofs, register_count);
        }
    }
cleanup:
    ruda_mir_range_analysis_dispose(&analysis);
    ruda_deallocate(&function->allocator, proofs, bytes);
    return status;
}
