#include "c_emitter_internal.h"

#include <string.h>

typedef struct MarimayNativeCRelations {
    size_t register_count;
    size_t relation_count;
    unsigned char *strict;
    unsigned char *nonnegative;
} MarimayNativeCRelations;

static bool mmay_native_c_relation_constant(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    size_t depth,
    int64_t *result
) {
    bool found = false;
    int64_t value = 0;
    size_t block_index = 0;
    if (function == NULL || result == NULL
        || depth > function->register_types.length) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            int64_t candidate = 0;
            if (instruction == NULL) return false;
            if (instruction->target != register_id) continue;
            if (instruction->kind == MMAY_MIR_CONSTANT_I64) {
                candidate = instruction->integer;
            } else if (instruction->kind == MMAY_MIR_COPY) {
                if (!mmay_native_c_relation_constant(
                        function, instruction->first, depth + 1, &candidate
                    )) return false;
            } else {
                return false;
            }
            if (found && candidate != value) return false;
            found = true;
            value = candidate;
        }
    }
    if (found) *result = value;
    return found;
}

static size_t mmay_native_c_relation_cell(
    const MarimayNativeCRelations *facts,
    MarimayMirRegister left,
    MarimayMirRegister right
) {
    return (size_t)left * facts->register_count + (size_t)right;
}

static bool mmay_native_c_relation_has(
    const MarimayNativeCRelations *facts,
    MarimayMirRegister left,
    MarimayMirRegister right
) {
    return left < facts->register_count && right < facts->register_count
        && facts->strict[mmay_native_c_relation_cell(facts, left, right)] != 0;
}

static void mmay_native_c_relation_add(
    MarimayNativeCRelations *facts,
    MarimayMirRegister left,
    MarimayMirRegister right
) {
    size_t lower = 0, upper = 0;
    if (left >= facts->register_count || right >= facts->register_count
        || left == right) return;
    for (lower = 0; lower < facts->register_count; lower += 1) {
        if (lower != left && !mmay_native_c_relation_has(
                facts, (MarimayMirRegister)lower, left
            )) continue;
        for (upper = 0; upper < facts->register_count; upper += 1) {
            if (upper != right && !mmay_native_c_relation_has(
                    facts, right, (MarimayMirRegister)upper
                )) continue;
            if (lower != upper) {
                facts->strict[mmay_native_c_relation_cell(
                    facts, (MarimayMirRegister)lower,
                    (MarimayMirRegister)upper
                )] = 1;
            }
        }
    }
}

static const MarimayMirInstruction *mmay_native_c_relation_definition(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    const MarimayMirInstruction *result = NULL;
    size_t block_index = 0;
    if (function == NULL) return NULL;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) return NULL;
            if (instruction->target != register_id) continue;
            if (result != NULL) return NULL;
            result = instruction;
        }
    }
    return result;
}

static void mmay_native_c_relation_invalidate(
    MarimayNativeCRelations *facts,
    MarimayMirRegister target
) {
    size_t register_index = 0;
    if (target >= facts->register_count) return;
    facts->nonnegative[target] = 0;
    for (register_index = 0; register_index < facts->register_count;
        register_index += 1) {
        facts->strict[mmay_native_c_relation_cell(
            facts, target, (MarimayMirRegister)register_index
        )] = 0;
        facts->strict[mmay_native_c_relation_cell(
            facts, (MarimayMirRegister)register_index, target
        )] = 0;
    }
}

static void mmay_native_c_relation_copy(
    MarimayNativeCRelations *facts,
    MarimayMirRegister target,
    MarimayMirRegister source
) {
    size_t register_index = 0;
    bool nonnegative = source < facts->register_count
        && facts->nonnegative[source] != 0;
    if (target >= facts->register_count || source >= facts->register_count) {
        return;
    }
    if (target == source) return;
    mmay_native_c_relation_invalidate(facts, target);
    facts->nonnegative[target] = nonnegative ? 1 : 0;
    for (register_index = 0; register_index < facts->register_count;
        register_index += 1) {
        if (mmay_native_c_relation_has(
                facts, source, (MarimayMirRegister)register_index
            )) {
            mmay_native_c_relation_add(
                facts, target, (MarimayMirRegister)register_index
            );
        }
        if (mmay_native_c_relation_has(
                facts, (MarimayMirRegister)register_index, source
            )) {
            mmay_native_c_relation_add(
                facts, (MarimayMirRegister)register_index, target
            );
        }
    }
}

static bool mmay_native_c_relation_subtract_nonnegative(
    const MarimayMirFunction *function,
    const MarimayNativeCRelations *facts,
    MarimayMirRegister left,
    MarimayMirRegister right
) {
    int64_t amount = -1;
    size_t register_index = 0;
    if (!mmay_native_c_relation_constant(
            function, right, 0, &amount
        ) || amount < 0) return false;
    if (amount == 0) {
        return left < facts->register_count
            && facts->nonnegative[left] != 0;
    }
    if (amount != 1) return false;
    for (register_index = 0; register_index < facts->register_count;
        register_index += 1) {
        int64_t zero = 1;
        if (mmay_native_c_relation_has(
                facts, (MarimayMirRegister)register_index, left
            ) && mmay_native_c_relation_constant(
                function, (MarimayMirRegister)register_index, 0, &zero
            ) && zero == 0) return true;
    }
    return false;
}

static bool mmay_native_c_relation_transfer(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayNativeCRelations *facts
) {
    size_t register_index = 0;
    int64_t value = 0;
    bool source_nonnegative = false;
    bool subtract_nonnegative = false;
    bool preserves_upper = false;
    int64_t positive_add = 0;
    if (instruction == NULL || facts == NULL) return false;
    if (instruction->target >= facts->register_count) return true;
    if (instruction->kind == MMAY_MIR_COPY) {
        mmay_native_c_relation_copy(
            facts, instruction->target, instruction->first
        );
        return true;
    }
    source_nonnegative = instruction->first < facts->register_count
        && facts->nonnegative[instruction->first] != 0;
    subtract_nonnegative =
        (instruction->kind == MMAY_MIR_SUBTRACT_CHECKED_I64
            || instruction->kind == MMAY_MIR_SUBTRACT_PROVEN_I64)
        && mmay_native_c_relation_subtract_nonnegative(
            function, facts, instruction->first, instruction->second
        );
    preserves_upper = instruction->target != instruction->first
        && (instruction->kind == MMAY_MIR_SUBTRACT_CHECKED_I64
            || instruction->kind == MMAY_MIR_SUBTRACT_PROVEN_I64)
        && mmay_native_c_relation_constant(
            function, instruction->second, 0, &value
        ) && value >= 0;
    mmay_native_c_relation_invalidate(facts, instruction->target);
    if (preserves_upper) {
        for (register_index = 0; register_index < facts->register_count;
            register_index += 1) {
            if (mmay_native_c_relation_has(
                    facts, instruction->first,
                    (MarimayMirRegister)register_index
                )) {
                mmay_native_c_relation_add(
                    facts, instruction->target,
                    (MarimayMirRegister)register_index
                );
            }
        }
    }
    switch (instruction->kind) {
        case MMAY_MIR_CONSTANT_I64:
            facts->nonnegative[instruction->target] =
                instruction->integer >= 0 ? 1 : 0;
            break;
        case MMAY_MIR_CONSTANT_BOOL:
        case MMAY_MIR_ARRAY_LENGTH:
            facts->nonnegative[instruction->target] = 1;
            break;
        case MMAY_MIR_ADD_CHECKED_I64:
        case MMAY_MIR_ADD_PROVEN_I64: {
            const MarimayMirInstruction *source_definition =
                mmay_native_c_relation_definition(
                    function, instruction->first
                );
            facts->nonnegative[instruction->target] = source_nonnegative
                && instruction->second < facts->register_count
                && facts->nonnegative[instruction->second] != 0 ? 1 : 0;
            if (source_nonnegative
                && mmay_native_c_relation_constant(
                    function, instruction->second, 0, &positive_add
                ) && positive_add > 0) {
                mmay_native_c_relation_add(
                    facts, instruction->first, instruction->target
                );
                if (source_definition != NULL
                    && (source_definition->kind
                            == MMAY_MIR_MULTIPLY_CHECKED_I64
                        || source_definition->kind
                            == MMAY_MIR_MULTIPLY_PROVEN_I64)) {
                    int64_t factor = 0;
                    if (mmay_native_c_relation_constant(
                            function, source_definition->second, 0, &factor
                        ) && factor >= 1
                        && source_definition->first < facts->register_count
                        && facts->nonnegative[
                            source_definition->first] != 0) {
                        mmay_native_c_relation_add(
                            facts, source_definition->first,
                            instruction->target
                        );
                    }
                }
            }
            break;
        }
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
        case MMAY_MIR_SUBTRACT_PROVEN_I64:
            facts->nonnegative[instruction->target] =
                subtract_nonnegative ? 1 : 0;
            if (mmay_native_c_relation_constant(
                    function, instruction->second, 0, &value
                ) && value > 0) {
                mmay_native_c_relation_add(
                    facts, instruction->target, instruction->first
                );
            }
            break;
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
        case MMAY_MIR_MULTIPLY_PROVEN_I64:
            facts->nonnegative[instruction->target] = source_nonnegative
                && instruction->second < facts->register_count
                && facts->nonnegative[instruction->second] != 0 ? 1 : 0;
            break;
        default:
            break;
    }
    return true;
}

static const MarimayMirInstruction *mmay_native_c_relation_condition(
    const MarimayMirBlock *block
) {
    size_t cursor = block != NULL ? block->instructions.length : 0;
    if (block == NULL || block->terminator.kind != MMAY_MIR_BRANCH) {
        return NULL;
    }
    while (cursor != 0) {
        const MarimayMirInstruction *instruction = NULL;
        cursor -= 1;
        instruction = mmay_vector_at_const(&block->instructions, cursor);
        if (instruction != NULL
            && instruction->target == block->terminator.value) {
            return instruction;
        }
    }
    return NULL;
}

static const MarimayMirInstruction *mmay_native_c_relation_local_source(
    const MarimayMirBlock *block,
    MarimayMirRegister register_id,
    size_t depth
) {
    size_t cursor = block != NULL ? block->instructions.length : 0;
    if (block == NULL || depth > block->instructions.length) return NULL;
    while (cursor != 0) {
        const MarimayMirInstruction *instruction = NULL;
        cursor -= 1;
        instruction = mmay_vector_at_const(&block->instructions, cursor);
        if (instruction == NULL || instruction->target != register_id) {
            continue;
        }
        return instruction->kind == MMAY_MIR_COPY
            ? mmay_native_c_relation_local_source(
                block, instruction->first, depth + 1
            ) : instruction;
    }
    return NULL;
}

static void mmay_native_c_relation_refine_edge(
    const MarimayMirFunction *function,
    const MarimayMirBlock *predecessor,
    MarimayMirBlockId successor,
    MarimayNativeCRelations *facts
) {
    const MarimayMirInstruction *condition =
        mmay_native_c_relation_condition(predecessor);
    bool true_edge = predecessor != NULL
        && predecessor->terminator.target == successor;
    int64_t zero = 1;
    if (condition == NULL
        || (condition->kind != MMAY_MIR_LESS_I64
            && condition->kind != MMAY_MIR_LESS_EQUAL_I64)) return;
    if (condition->kind == MMAY_MIR_LESS_I64 && true_edge) {
        mmay_native_c_relation_add(
            facts, condition->first, condition->second
        );
        if (mmay_native_c_relation_constant(
                function, condition->first, 0, &zero
            ) && zero == 0 && condition->second < facts->register_count) {
            facts->nonnegative[condition->second] = 1;
        }
    } else if (condition->kind == MMAY_MIR_LESS_EQUAL_I64 && !true_edge) {
        mmay_native_c_relation_add(
            facts, condition->second, condition->first
        );
    }
    zero = 1;
    if (condition->kind == MMAY_MIR_LESS_I64 && !true_edge
        && mmay_native_c_relation_constant(
            function, condition->second, 0, &zero
        ) && zero == 0 && condition->first < facts->register_count) {
        facts->nonnegative[condition->first] = 1;
    }
    zero = 1;
    if (condition->kind == MMAY_MIR_LESS_EQUAL_I64 && true_edge
        && mmay_native_c_relation_constant(
            function, condition->first, 0, &zero
        ) && zero == 0 && condition->second < facts->register_count) {
        facts->nonnegative[condition->second] = 1;
    }
}

static bool mmay_native_c_relation_predecessor(
    const MarimayMirBlock *block,
    MarimayMirBlockId successor
) {
    return block != NULL
        && ((block->terminator.kind == MMAY_MIR_GOTO
                && block->terminator.target == successor)
            || (block->terminator.kind == MMAY_MIR_BRANCH
                && (block->terminator.target == successor
                    || block->terminator.otherwise == successor)));
}

static bool mmay_native_c_relation_joined_edge(
    const MarimayMirFunction *function,
    const unsigned char *outgoing_strict,
    const unsigned char *outgoing_nonnegative,
    const unsigned char *ready,
    size_t relation_count,
    size_t register_count,
    MarimayMirBlockId predecessor_id,
    MarimayMirBlockId successor,
    unsigned char *edge_strict,
    unsigned char *edge_nonnegative
) {
    const MarimayMirBlock *predecessor = mmay_vector_at_const(
        &function->blocks, predecessor_id
    );
    size_t candidate_index = 0;
    bool desired = predecessor != NULL
        && predecessor->terminator.target == successor;
    bool merged = false, excluded = false;
    if (predecessor == NULL
        || predecessor->terminator.kind != MMAY_MIR_BRANCH
        || mmay_native_c_relation_condition(predecessor) != NULL) {
        return false;
    }
    for (candidate_index = 0;
        candidate_index < function->blocks.length;
        candidate_index += 1) {
        const MarimayMirBlock *candidate = mmay_vector_at_const(
            &function->blocks, candidate_index
        );
        const MarimayMirInstruction *source = NULL;
        bool feasible = true;
        if (!mmay_native_c_relation_predecessor(
                candidate, predecessor_id
            )) continue;
        if (candidate->terminator.kind != MMAY_MIR_GOTO
            || candidate->terminator.target != predecessor_id) return false;
        if (ready[candidate_index] == 0) return false;
        source = mmay_native_c_relation_local_source(
            candidate, predecessor->terminator.value, 0
        );
        if (source == NULL) return false;
        if (source->kind == MMAY_MIR_CONSTANT_BOOL) {
            feasible = source->boolean == desired;
        } else if (source->kind != MMAY_MIR_LESS_I64
            && source->kind != MMAY_MIR_LESS_EQUAL_I64
            && source->kind != MMAY_MIR_EQUAL_I64
            && source->kind != MMAY_MIR_EQUAL_BOOL) {
            return false;
        }
        if (!feasible) {
            excluded = true;
            continue;
        }
        if (!merged) {
            memcpy(
                edge_strict,
                outgoing_strict + candidate_index * relation_count,
                relation_count
            );
            memcpy(
                edge_nonnegative,
                outgoing_nonnegative + candidate_index * register_count,
                register_count
            );
            merged = true;
        } else {
            size_t cell = 0;
            const unsigned char *candidate_strict = outgoing_strict
                + candidate_index * relation_count;
            const unsigned char *candidate_lower = outgoing_nonnegative
                + candidate_index * register_count;
            for (cell = 0; cell < relation_count; cell += 1) {
                edge_strict[cell] = edge_strict[cell]
                    && candidate_strict[cell];
            }
            for (cell = 0; cell < register_count; cell += 1) {
                edge_nonnegative[cell] = edge_nonnegative[cell]
                    && candidate_lower[cell];
            }
        }
    }
    return merged && excluded;
}

static bool mmay_native_c_program_has_interface_call(
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < function_count;
        function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t block_index = 0;
        for (block_index = 0;
            function != NULL && block_index < function->blocks.length;
            block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(
                &function->blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0;
                block != NULL
                    && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const MarimayMirInstruction *instruction = mmay_vector_at_const(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && instruction->kind == MMAY_MIR_CALL_INTERFACE) {
                    return true;
                }
            }
        }
    }
    return false;
}

static bool mmay_native_c_parameter_nonnegative(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirFunction *callee,
    size_t parameter_index
) {
    size_t function_index = 0;
    bool found = false;
    if (functions == NULL || callee == NULL
        || parameter_index >= callee->parameter_count
        || mmay_native_c_program_has_interface_call(
            functions, function_count
        )) return false;
    for (function_index = 0; function_index < function_count;
        function_index += 1) {
        const MarimayMirFunction *caller = functions[function_index];
        size_t call_index = 0;
        for (call_index = 0;
            caller != NULL && call_index < caller->calls.length;
            call_index += 1) {
            const MarimayMirCall *call = mmay_vector_at_const(
                &caller->calls, call_index
            );
            const MarimayMirCallArgument *argument = NULL;
            int64_t value = -1;
            if (call == NULL || !mmay_symbol_id_equal(
                    call->callee, callee->symbol
                )) continue;
            if (parameter_index >= call->argument_count) return false;
            argument = mmay_vector_at_const(
                &caller->call_arguments,
                call->argument_start + parameter_index
            );
            if (argument == NULL || argument->type != MMAY_TYPE_I64
                || !mmay_native_c_relation_constant(
                    caller, argument->register_id, 0, &value
                ) || value < 0) return false;
            found = true;
        }
    }
    return found;
}

static bool mmay_native_c_relations_analyze(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *target_instruction,
    MarimayNativeCRelations *result
) {
    unsigned char *incoming_strict = NULL, *outgoing_strict = NULL;
    unsigned char *incoming_nonnegative = NULL, *outgoing_nonnegative = NULL;
    unsigned char *edge_strict = NULL, *edge_nonnegative = NULL;
    unsigned char *ready = NULL;
    size_t block_count = function->blocks.length;
    size_t register_count = function->register_types.length;
    size_t relation_count = 0, strict_count = 0, lower_count = 0;
    size_t block_index = 0, iteration = 0;
    MarimayMirBlockId target_block = MMAY_MIR_NO_BLOCK;
    size_t target_index = 0;
    bool changed = true, success = false;
    if (result == NULL || !mmay_size_multiply(
            register_count, register_count, &relation_count
        ) || !mmay_size_multiply(
            block_count, relation_count, &strict_count
        ) || !mmay_size_multiply(
            block_count, register_count, &lower_count
        )) return false;
    memset(result, 0, sizeof(*result));
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            if (mmay_vector_at_const(
                    &block->instructions, instruction_index
                ) == target_instruction) {
                target_block = (MarimayMirBlockId)block_index;
                target_index = instruction_index;
            }
        }
    }
    if (target_block == MMAY_MIR_NO_BLOCK) return false;
    incoming_strict = mmay_allocate_array(
        &function->allocator, strict_count, sizeof(*incoming_strict)
    );
    outgoing_strict = mmay_allocate_array(
        &function->allocator, strict_count, sizeof(*outgoing_strict)
    );
    incoming_nonnegative = mmay_allocate_array(
        &function->allocator, lower_count, sizeof(*incoming_nonnegative)
    );
    outgoing_nonnegative = mmay_allocate_array(
        &function->allocator, lower_count, sizeof(*outgoing_nonnegative)
    );
    ready = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*ready)
    );
    result->strict = mmay_allocate_array(
        &function->allocator, relation_count, sizeof(*result->strict)
    );
    result->nonnegative = mmay_allocate_array(
        &function->allocator, register_count, sizeof(*result->nonnegative)
    );
    edge_strict = mmay_allocate_array(
        &function->allocator, relation_count, sizeof(*edge_strict)
    );
    edge_nonnegative = mmay_allocate_array(
        &function->allocator, register_count, sizeof(*edge_nonnegative)
    );
    if (incoming_strict == NULL || outgoing_strict == NULL
        || incoming_nonnegative == NULL || outgoing_nonnegative == NULL
        || ready == NULL || result->strict == NULL
        || result->nonnegative == NULL || edge_strict == NULL
        || edge_nonnegative == NULL) goto cleanup;
    result->register_count = register_count;
    result->relation_count = relation_count;
    memset(incoming_strict, 0, strict_count * sizeof(*incoming_strict));
    memset(outgoing_strict, 0, strict_count * sizeof(*outgoing_strict));
    memset(incoming_nonnegative, 0,
        lower_count * sizeof(*incoming_nonnegative));
    memset(outgoing_nonnegative, 0,
        lower_count * sizeof(*outgoing_nonnegative));
    memset(ready, 0, block_count * sizeof(*ready));
    while (changed && iteration++ <= block_count * 4 + 4) {
        changed = false;
        for (block_index = 0; block_index < block_count; block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(
                &function->blocks, block_index
            );
            unsigned char *incoming_relation = incoming_strict
                + block_index * relation_count;
            unsigned char *incoming_lower = incoming_nonnegative
                + block_index * register_count;
            unsigned char *outgoing_relation = outgoing_strict
                + block_index * relation_count;
            unsigned char *outgoing_lower = outgoing_nonnegative
                + block_index * register_count;
            bool incoming_ready = block_index == 0;
            size_t predecessor_index = 0, instruction_index = 0;
            memset(result->strict, 0, relation_count);
            memset(result->nonnegative, 0, register_count);
            if (block_index == 0) {
                size_t parameter_index = 0;
                for (parameter_index = 0;
                    parameter_index < function->parameter_count;
                    parameter_index += 1) {
                    const MarimayTypeId *type = mmay_vector_at_const(
                        &function->register_types, parameter_index
                    );
                    if (type != NULL && *type == MMAY_TYPE_I64
                        && mmay_native_c_parameter_nonnegative(
                            functions, function_count, function,
                            parameter_index
                        )) {
                        result->nonnegative[parameter_index] = 1;
                    }
                }
            }
            for (predecessor_index = 0;
                predecessor_index < block_count;
                predecessor_index += 1) {
                const MarimayMirBlock *predecessor = mmay_vector_at_const(
                    &function->blocks, predecessor_index
                );
                unsigned char *predecessor_relation = outgoing_strict
                    + predecessor_index * relation_count;
                unsigned char *predecessor_lower = outgoing_nonnegative
                    + predecessor_index * register_count;
                MarimayNativeCRelations edge;
                if (ready[predecessor_index] == 0
                    || !mmay_native_c_relation_predecessor(
                    predecessor, (MarimayMirBlockId)block_index
                    )) continue;
                if (!mmay_native_c_relation_joined_edge(
                        function, outgoing_strict, outgoing_nonnegative,
                        ready, relation_count, register_count,
                        (MarimayMirBlockId)predecessor_index,
                        (MarimayMirBlockId)block_index,
                        edge_strict, edge_nonnegative
                    )) {
                    memcpy(
                        edge_strict, predecessor_relation, relation_count
                    );
                    memcpy(
                        edge_nonnegative, predecessor_lower, register_count
                    );
                }
                edge.register_count = register_count;
                edge.relation_count = relation_count;
                edge.strict = edge_strict;
                edge.nonnegative = edge_nonnegative;
                mmay_native_c_relation_refine_edge(
                    function, predecessor, (MarimayMirBlockId)block_index,
                    &edge
                );
                if (!incoming_ready) {
                    memcpy(result->strict, edge_strict,
                        relation_count);
                    memcpy(result->nonnegative, edge_nonnegative,
                        register_count);
                    incoming_ready = true;
                } else {
                    size_t cell = 0;
                    for (cell = 0; cell < relation_count; cell += 1) {
                        result->strict[cell] = result->strict[cell]
                            && edge_strict[cell];
                    }
                    for (cell = 0; cell < register_count; cell += 1) {
                        result->nonnegative[cell] =
                            result->nonnegative[cell]
                            && edge_nonnegative[cell];
                    }
                }
            }
            if (!incoming_ready || block == NULL) continue;
            if (memcmp(incoming_relation, result->strict,
                    relation_count) != 0
                || memcmp(incoming_lower, result->nonnegative,
                    register_count) != 0) {
                memcpy(incoming_relation, result->strict, relation_count);
                memcpy(incoming_lower, result->nonnegative, register_count);
                changed = true;
            }
            for (instruction_index = 0;
                instruction_index < block->instructions.length;
                instruction_index += 1) {
                const MarimayMirInstruction *instruction = mmay_vector_at_const(
                    &block->instructions, instruction_index
                );
                if (!mmay_native_c_relation_transfer(
                        function, instruction, result
                    )) goto cleanup;
            }
            if (ready[block_index] == 0
                || memcmp(outgoing_relation, result->strict,
                    relation_count) != 0
                || memcmp(outgoing_lower, result->nonnegative,
                    register_count) != 0) {
                memcpy(outgoing_relation, result->strict, relation_count);
                memcpy(outgoing_lower, result->nonnegative, register_count);
                ready[block_index] = 1;
                changed = true;
            }
        }
    }
    if (iteration > block_count * 4 + 5 || ready[target_block] == 0) {
        goto cleanup;
    }
    memcpy(result->strict,
        incoming_strict + target_block * relation_count, relation_count);
    memcpy(result->nonnegative,
        incoming_nonnegative + target_block * register_count, register_count);
    {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, target_block
        );
        for (block_index = 0; block_index < target_index;
            block_index += 1) {
            if (!mmay_native_c_relation_transfer(
                    function,
                    mmay_vector_at_const(&block->instructions, block_index),
                    result
                )) goto cleanup;
        }
    }
    success = true;
cleanup:
    mmay_deallocate(&function->allocator, incoming_strict, strict_count);
    mmay_deallocate(&function->allocator, outgoing_strict, strict_count);
    mmay_deallocate(
        &function->allocator, incoming_nonnegative, lower_count
    );
    mmay_deallocate(
        &function->allocator, outgoing_nonnegative, lower_count
    );
    mmay_deallocate(&function->allocator, ready, block_count);
    mmay_deallocate(
        &function->allocator, edge_strict, relation_count
    );
    mmay_deallocate(
        &function->allocator, edge_nonnegative, register_count
    );
    if (!success) {
        mmay_deallocate(
            &function->allocator, result->strict, relation_count
        );
        mmay_deallocate(
            &function->allocator, result->nonnegative, register_count
        );
        memset(result, 0, sizeof(*result));
    }
    return success;
}

static void mmay_native_c_relations_dispose(
    const MarimayMirFunction *function,
    MarimayNativeCRelations *facts
) {
    if (function == NULL || facts == NULL) return;
    mmay_deallocate(
        &function->allocator, facts->strict, facts->relation_count
    );
    mmay_deallocate(
        &function->allocator, facts->nonnegative, facts->register_count
    );
    memset(facts, 0, sizeof(*facts));
}

static bool mmay_native_c_array_size_unmodified(
    const MarimayMirFunction *function,
    MarimayMirRegister array_register
) {
    MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    if (function == NULL || !mmay_native_c_array_register_origin(
            function, array_register, &origin
        )) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block != NULL
            && block->terminator.kind == MMAY_MIR_CALL_NEVER) return false;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayMirRegister mutation_origin = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind == MMAY_MIR_CALL
                || instruction->kind == MMAY_MIR_CALL_INTERFACE) {
                return false;
            }
            if (instruction->kind != MMAY_MIR_ARRAY_PUSH
                && instruction->kind != MMAY_MIR_ARRAY_POP
                && instruction->kind != MMAY_MIR_ARRAY_TRUNCATE
                && instruction->kind != MMAY_MIR_ARRAY_CLEAR) continue;
            if (!mmay_native_c_array_register_origin(
                    function, instruction->first, &mutation_origin
                ) || mutation_origin == origin) return false;
        }
    }
    return true;
}

bool mmay_native_c_array_relational_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register
) {
    MarimayNativeCRelations facts;
    MarimayMirRegister array_origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    bool upper = false, result = false;
    if (function == NULL || instruction == NULL
        || index_register >= function->register_types.length
        || !mmay_native_c_array_size_unmodified(
            function, array_register
        ) || !mmay_native_c_array_register_origin(
            function, array_register, &array_origin
        ) || !mmay_native_c_relations_analyze(
            function, functions, function_count, instruction, &facts
        )) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *length = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayMirRegister length_origin = MMAY_MIR_NO_REGISTER;
            if (length != NULL && length->kind == MMAY_MIR_ARRAY_LENGTH
                && mmay_native_c_array_register_origin(
                    function, length->first, &length_origin
                ) && length_origin == array_origin) {
                if (mmay_native_c_relation_has(
                    &facts, index_register, length->target
                )) upper = true;
            }
        }
    }
    result = facts.nonnegative[index_register] != 0 && upper;
    mmay_native_c_relations_dispose(function, &facts);
    return result;
}

bool mmay_native_c_subtract_one_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction
) {
    MarimayNativeCRelations facts;
    size_t register_index = 0;
    int64_t amount = 0;
    bool result = false;
    if (function == NULL || instruction == NULL
        || instruction->kind != MMAY_MIR_SUBTRACT_CHECKED_I64
        || instruction->first >= function->register_types.length
        || !mmay_native_c_relation_constant(
            function, instruction->second, 0, &amount
        ) || amount != 1
        || !mmay_native_c_relations_analyze(
            function, functions, function_count, instruction, &facts
        )) return false;
    for (register_index = 0;
        register_index < facts.register_count;
        register_index += 1) {
        int64_t zero = 1;
        if (mmay_native_c_relation_has(
                &facts,
                (MarimayMirRegister)register_index,
                instruction->first
            ) && mmay_native_c_relation_constant(
                function, (MarimayMirRegister)register_index, 0, &zero
            ) && zero == 0) {
            result = true;
            break;
        }
    }
    mmay_native_c_relations_dispose(function, &facts);
    return result;
}

bool mmay_native_c_add_one_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction
) {
    MarimayNativeCRelations facts;
    const MarimayTypeId *operand_type = NULL;
    MarimayMirRegister operand = MMAY_MIR_NO_REGISTER;
    size_t register_index = 0;
    int64_t amount = 0;
    bool result = false;
    if (function == NULL || instruction == NULL
        || instruction->kind != MMAY_MIR_ADD_CHECKED_I64) return false;
    if (mmay_native_c_relation_constant(
            function, instruction->second, 0, &amount
        ) && amount == 1) {
        operand = instruction->first;
    } else if (mmay_native_c_relation_constant(
            function, instruction->first, 0, &amount
        ) && amount == 1) {
        operand = instruction->second;
    } else {
        return false;
    }
    operand_type = mmay_vector_at_const(
        &function->register_types, operand
    );
    if (operand_type == NULL || *operand_type != MMAY_TYPE_I64
        || !mmay_native_c_relations_analyze(
            function, functions, function_count, instruction, &facts
        )) return false;
    for (register_index = 0;
        register_index < facts.register_count;
        register_index += 1) {
        const MarimayTypeId *upper_type = mmay_vector_at_const(
            &function->register_types, register_index
        );
        if (upper_type != NULL && *upper_type == MMAY_TYPE_I64
            && mmay_native_c_relation_has(
                &facts, operand, (MarimayMirRegister)register_index
            )) {
            result = true;
            break;
        }
    }
    mmay_native_c_relations_dispose(function, &facts);
    return result;
}

bool mmay_native_c_negate_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    int64_t value = 0;
    return function != NULL && instruction != NULL
        && instruction->kind == MMAY_MIR_NEGATE_CHECKED_I64
        && mmay_native_c_relation_constant(
            function, instruction->first, 0, &value
        ) && value != INT64_MIN;
}

bool mmay_native_c_divisor_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    int64_t divisor = 0;
    return function != NULL && instruction != NULL
        && (instruction->kind == MMAY_MIR_DIVIDE_CHECKED_I64
            || instruction->kind == MMAY_MIR_REMAINDER_CHECKED_I64)
        && mmay_native_c_relation_constant(
            function, instruction->second, 0, &divisor
        ) && divisor != 0 && divisor != -1;
}
