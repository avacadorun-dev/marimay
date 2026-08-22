#include "c_emitter_internal.h"

#include <string.h>

typedef enum MarimayNativeCBound {
    MMAY_NATIVE_C_BOUND_LOWER,
    MMAY_NATIVE_C_BOUND_UPPER
} MarimayNativeCBound;

static bool mmay_native_c_scalar_record_origin(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *definition,
    MarimayMirRegister *result
) {
    const MarimayMirRecord *record = function != NULL && definition != NULL
        && definition->kind == MMAY_MIR_RECORD_GET
        ? mmay_vector_at_const(&function->records, definition->record) : NULL;
    const MarimayMirRecordField *field = record != NULL
        && definition->field < record->field_count
        ? mmay_vector_at_const(
            &function->record_fields,
            record->field_start + definition->field
        ) : NULL;
    MarimayMirRegister canonical = definition != NULL
        ? definition->target : MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    if (function == NULL || definition == NULL || result == NULL
        || field == NULL || field->is_mutable) return false;
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
            if (instruction == NULL) return false;
            if (instruction->kind == MMAY_MIR_RECORD_GET
                && instruction->first == definition->first
                && instruction->record == definition->record
                && instruction->field == definition->field
                && instruction->target < canonical) {
                canonical = instruction->target;
            }
        }
    }
    *result = canonical;
    return true;
}

static bool mmay_native_c_scalar_origin(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    size_t depth,
    MarimayMirRegister *result
) {
    bool found = false;
    MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    if (function == NULL || result == NULL
        || register_id >= function->register_types.length
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
            MarimayMirRegister candidate = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->target != register_id) continue;
            if (instruction->kind != MMAY_MIR_COPY
                || !mmay_native_c_scalar_origin(
                    function, instruction->first, depth + 1, &candidate
                )) {
                if (!mmay_native_c_scalar_record_origin(
                        function, instruction, result
                    )) *result = register_id;
                return true;
            }
            if (found && candidate != origin) {
                *result = register_id;
                return true;
            }
            found = true;
            origin = candidate;
        }
    }
    *result = found ? origin : register_id;
    return true;
}

static bool mmay_native_c_scalar_constant(
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
                if (!mmay_native_c_scalar_constant(
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

static bool mmay_native_c_instruction_location(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirBlockId *block_id,
    size_t *instruction_index
) {
    size_t block_index = 0;
    if (function == NULL || instruction == NULL || block_id == NULL
        || instruction_index == NULL) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t index = 0;
        if (block == NULL) return false;
        for (index = 0; index < block->instructions.length; index += 1) {
            if (mmay_vector_at_const(&block->instructions, index)
                    == instruction) {
                *block_id = (MarimayMirBlockId)block_index;
                *instruction_index = index;
                return true;
            }
        }
    }
    return false;
}

static const MarimayMirInstruction *mmay_native_c_local_definition(
    const MarimayMirBlock *block,
    size_t before,
    MarimayMirRegister register_id
) {
    size_t cursor = before;
    if (block == NULL || cursor > block->instructions.length) return NULL;
    while (cursor != 0) {
        const MarimayMirInstruction *instruction = NULL;
        cursor -= 1;
        instruction = mmay_vector_at_const(&block->instructions, cursor);
        if (instruction != NULL && instruction->target == register_id) {
            return instruction;
        }
    }
    return NULL;
}

static const MarimayMirInstruction *mmay_native_c_local_source(
    const MarimayMirBlock *block,
    size_t before,
    MarimayMirRegister register_id,
    size_t depth
) {
    const MarimayMirInstruction *definition = NULL;
    if (block == NULL || depth > block->instructions.length) return NULL;
    definition = mmay_native_c_local_definition(block, before, register_id);
    if (definition != NULL && definition->kind == MMAY_MIR_COPY) {
        size_t index = (size_t)(definition - (
            const MarimayMirInstruction *)block->instructions.data
        );
        return mmay_native_c_local_source(
            block, index, definition->first, depth + 1
        );
    }
    return definition;
}

static bool mmay_native_c_comparison_matches(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *comparison,
    MarimayNativeCBound bound,
    MarimayMirRegister index_origin,
    MarimayMirRegister count_origin,
    bool *safe_when_true
) {
    MarimayMirRegister first = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister second = MMAY_MIR_NO_REGISTER;
    int64_t zero = 1;
    if (comparison == NULL || safe_when_true == NULL
        || (comparison->kind != MMAY_MIR_LESS_I64
            && comparison->kind != MMAY_MIR_LESS_EQUAL_I64)
        || !mmay_native_c_scalar_origin(
            function, comparison->first, 0, &first
        ) || !mmay_native_c_scalar_origin(
            function, comparison->second, 0, &second
        )) return false;
    if (bound == MMAY_NATIVE_C_BOUND_LOWER) {
        if (comparison->kind == MMAY_MIR_LESS_I64
            && first == index_origin
            && mmay_native_c_scalar_constant(
                function, comparison->second, 0, &zero
            ) && zero == 0) {
            *safe_when_true = false;
            return true;
        }
        if (comparison->kind == MMAY_MIR_LESS_EQUAL_I64
            && second == index_origin
            && mmay_native_c_scalar_constant(
                function, comparison->first, 0, &zero
            ) && zero == 0) {
            *safe_when_true = true;
            return true;
        }
        return false;
    }
    if (comparison->kind == MMAY_MIR_LESS_EQUAL_I64
        && first == count_origin && second == index_origin) {
        *safe_when_true = false;
        return true;
    }
    if (comparison->kind == MMAY_MIR_LESS_I64
        && first == index_origin && second == count_origin) {
        *safe_when_true = true;
        return true;
    }
    return false;
}

static bool mmay_native_c_push_successor_skipping_edge(
    MarimayMirBlockId predecessor,
    MarimayMirBlockId successor,
    MarimayMirBlockId skip_from,
    MarimayMirBlockId skip_to,
    size_t block_count,
    unsigned char *visited,
    MarimayMirBlockId *queue,
    size_t *tail
) {
    if (predecessor == skip_from && successor == skip_to) return true;
    if (successor >= block_count) return false;
    if (visited[successor] == 0) {
        visited[successor] = 1;
        queue[*tail] = successor;
        *tail += 1;
    }
    return true;
}

static bool mmay_native_c_edge_dominates(
    const MarimayMirFunction *function,
    MarimayMirBlockId from,
    MarimayMirBlockId to,
    MarimayMirBlockId target
) {
    unsigned char *visited = NULL;
    MarimayMirBlockId *queue = NULL;
    size_t block_count = function != NULL ? function->blocks.length : 0;
    size_t queue_bytes = 0, head = 0, tail = 0;
    bool result = false, valid = false;
    if (target >= block_count || from >= block_count || to >= block_count
        || !mmay_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = mmay_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    visited[0] = 1;
    queue[tail++] = 0;
    while (head < tail && visited[target] == 0) {
        MarimayMirBlockId block_id = queue[head++];
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_id
        );
        if (block == NULL) goto cleanup;
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            if (!mmay_native_c_push_successor_skipping_edge(
                    block_id, block->terminator.target, from, to,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            if (!mmay_native_c_push_successor_skipping_edge(
                    block_id, block->terminator.target, from, to,
                    block_count, visited, queue, &tail
                ) || !mmay_native_c_push_successor_skipping_edge(
                    block_id, block->terminator.otherwise, from, to,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        }
    }
    result = visited[target] == 0;
    valid = true;
cleanup:
    mmay_deallocate(&function->allocator, queue, queue_bytes);
    mmay_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    return valid && result;
}

static bool mmay_native_c_path_preserves_registers(
    const MarimayMirFunction *function,
    MarimayMirBlockId start,
    MarimayMirBlockId target,
    size_t target_instruction,
    MarimayMirBlockId stop,
    MarimayMirRegister first,
    MarimayMirRegister second
) {
    unsigned char *visited = NULL;
    unsigned char *reaches_target = NULL;
    MarimayMirBlockId *queue = NULL;
    size_t block_count = function->blocks.length;
    size_t queue_bytes = 0, head = 0, tail = 0;
    bool valid = false;
    if (start >= block_count || target >= block_count
        || !mmay_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    reaches_target = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*reaches_target)
    );
    queue = mmay_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || reaches_target == NULL || queue == NULL) {
        goto cleanup;
    }
    memset(visited, 0, block_count * sizeof(*visited));
    memset(reaches_target, 0, block_count * sizeof(*reaches_target));
    reaches_target[target] = 1;
    queue[tail++] = target;
    while (head < tail) {
        MarimayMirBlockId successor = queue[head++];
        size_t predecessor_index = 0;
        for (predecessor_index = 0; predecessor_index < block_count;
            predecessor_index += 1) {
            const MarimayMirBlock *predecessor = mmay_vector_at_const(
                &function->blocks, predecessor_index
            );
            bool enters = predecessor != NULL
                && predecessor_index != stop
                && ((predecessor->terminator.kind == MMAY_MIR_GOTO
                        && predecessor->terminator.target == successor)
                    || (predecessor->terminator.kind == MMAY_MIR_BRANCH
                        && (predecessor->terminator.target == successor
                            || predecessor->terminator.otherwise
                                == successor)));
            if (enters && reaches_target[predecessor_index] == 0) {
                reaches_target[predecessor_index] = 1;
                queue[tail++] = (MarimayMirBlockId)predecessor_index;
            }
        }
    }
    head = 0;
    tail = 0;
    if (reaches_target[start] == 0) goto cleanup;
    visited[start] = 1;
    queue[tail++] = start;
    while (head < tail) {
        MarimayMirBlockId block_id = queue[head++];
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_id
        );
        size_t instruction_index = 0;
        size_t limit = block_id == target
            ? target_instruction : block != NULL
                ? block->instructions.length : 0;
        if (block == NULL || block_id == stop) goto cleanup;
        for (instruction_index = 0; instruction_index < limit;
            instruction_index += 1) {
            const MarimayMirInstruction *current = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (current == NULL || current->target == first
                || current->target == second) goto cleanup;
        }
        if (block_id == target) continue;
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            MarimayMirBlockId successor = block->terminator.target;
            if (successor < block_count && successor != stop
                && reaches_target[successor] != 0
                && visited[successor] == 0) {
                visited[successor] = 1;
                queue[tail++] = successor;
            }
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            MarimayMirBlockId successors[2] = {
                block->terminator.target, block->terminator.otherwise
            };
            size_t successor_index = 0;
            for (successor_index = 0; successor_index < 2;
                successor_index += 1) {
                MarimayMirBlockId successor = successors[successor_index];
                if (successor < block_count && successor != stop
                    && reaches_target[successor] != 0
                    && visited[successor] == 0) {
                    visited[successor] = 1;
                    queue[tail++] = successor;
                }
            }
        }
    }
    valid = visited[target] != 0;
cleanup:
    mmay_deallocate(&function->allocator, queue, queue_bytes);
    mmay_deallocate(
        &function->allocator, reaches_target,
        block_count * sizeof(*reaches_target)
    );
    mmay_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    return valid;
}

static bool mmay_native_c_short_circuit_guard_proves(
    const MarimayMirFunction *function,
    MarimayMirBlockId target,
    size_t target_instruction,
    MarimayNativeCBound bound,
    MarimayMirRegister index_origin,
    MarimayMirRegister count_origin
) {
    size_t comparison_index = 0;
    for (comparison_index = 0;
        comparison_index < function->blocks.length;
        comparison_index += 1) {
        const MarimayMirBlock *comparison = mmay_vector_at_const(
            &function->blocks, comparison_index
        );
        const MarimayMirInstruction *condition = NULL;
        bool safe_when_true = false;
        MarimayMirBlockId unsafe = MMAY_MIR_NO_BLOCK;
        const MarimayMirBlock *unsafe_block = NULL;
        const MarimayMirBlock *join = NULL;
        MarimayMirBlockId join_id = MMAY_MIR_NO_BLOCK;
        const MarimayMirInstruction *assigned = NULL;
        bool assigned_value = false;
        MarimayMirBlockId safe_join = MMAY_MIR_NO_BLOCK;
        if (comparison == NULL
            || comparison->terminator.kind != MMAY_MIR_BRANCH) continue;
        condition = mmay_native_c_local_source(
            comparison, comparison->instructions.length,
            comparison->terminator.value, 0
        );
        if (!mmay_native_c_comparison_matches(
                function, condition, bound, index_origin, count_origin,
                &safe_when_true
            )) continue;
        unsafe = safe_when_true
            ? comparison->terminator.otherwise
            : comparison->terminator.target;
        unsafe_block = mmay_vector_at_const(&function->blocks, unsafe);
        if (unsafe_block == NULL
            || unsafe_block->terminator.kind != MMAY_MIR_GOTO) continue;
        join_id = unsafe_block->terminator.target;
        join = mmay_vector_at_const(&function->blocks, join_id);
        if (join == NULL || join->terminator.kind != MMAY_MIR_BRANCH) continue;
        assigned = mmay_native_c_local_source(
            unsafe_block, unsafe_block->instructions.length,
            join->terminator.value, 0
        );
        if (assigned == NULL || assigned->kind != MMAY_MIR_CONSTANT_BOOL) {
            continue;
        }
        assigned_value = assigned->boolean;
        safe_join = assigned_value
            ? join->terminator.otherwise : join->terminator.target;
        if (mmay_native_c_edge_dominates(
                function, join_id, safe_join, target
            ) && mmay_native_c_path_preserves_registers(
                function, safe_join, target, target_instruction, join_id,
                index_origin, count_origin
            )) return true;
    }
    return false;
}

static bool mmay_native_c_direct_guard_proves(
    const MarimayMirFunction *function,
    MarimayMirBlockId target,
    size_t target_instruction,
    MarimayNativeCBound bound,
    MarimayMirRegister index_origin,
    MarimayMirRegister count_origin
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        const MarimayMirInstruction *condition = NULL;
        bool safe_when_true = false;
        MarimayMirBlockId safe = MMAY_MIR_NO_BLOCK;
        if (block == NULL || block->terminator.kind != MMAY_MIR_BRANCH) {
            continue;
        }
        condition = mmay_native_c_local_source(
            block, block->instructions.length,
            block->terminator.value, 0
        );
        if (!mmay_native_c_comparison_matches(
                function, condition, bound, index_origin, count_origin,
                &safe_when_true
            )) continue;
        safe = safe_when_true
            ? block->terminator.target : block->terminator.otherwise;
        if (mmay_native_c_edge_dominates(
                function, (MarimayMirBlockId)block_index, safe, target
            ) && mmay_native_c_path_preserves_registers(
                function, safe, target, target_instruction,
                (MarimayMirBlockId)block_index,
                index_origin, count_origin
            )) return true;
    }
    return false;
}

static bool mmay_native_c_block_assigns_true(
    const MarimayMirBlock *block,
    MarimayMirRegister register_id
) {
    const MarimayMirInstruction *source = mmay_native_c_local_source(
        block, block != NULL ? block->instructions.length : 0,
        register_id, 0
    );
    return source != NULL && source->kind == MMAY_MIR_CONSTANT_BOOL
        && source->boolean;
}

static bool mmay_native_c_joined_guard_proves(
    const MarimayMirFunction *function,
    MarimayMirBlockId target,
    size_t target_instruction,
    MarimayNativeCBound bound,
    MarimayMirRegister index_origin,
    MarimayMirRegister count_origin
) {
    size_t comparison_block_index = 0;
    for (comparison_block_index = 0;
        comparison_block_index < function->blocks.length;
        comparison_block_index += 1) {
        const MarimayMirBlock *comparison_block = mmay_vector_at_const(
            &function->blocks, comparison_block_index
        );
        const MarimayMirBlock *join = NULL;
        const MarimayMirInstruction *source = NULL;
        MarimayMirBlockId join_id = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId safe = MMAY_MIR_NO_BLOCK;
        bool safe_when_true = false, all_other_true = true;
        size_t predecessor_index = 0;
        if (comparison_block == NULL
            || comparison_block->terminator.kind != MMAY_MIR_GOTO) continue;
        join_id = comparison_block->terminator.target;
        join = mmay_vector_at_const(&function->blocks, join_id);
        if (join == NULL || join->terminator.kind != MMAY_MIR_BRANCH) continue;
        source = mmay_native_c_local_source(
            comparison_block, comparison_block->instructions.length,
            join->terminator.value, 0
        );
        if (!mmay_native_c_comparison_matches(
                function, source, bound, index_origin, count_origin,
                &safe_when_true
            )) continue;
        for (predecessor_index = 0;
            predecessor_index < function->blocks.length;
            predecessor_index += 1) {
            const MarimayMirBlock *predecessor = mmay_vector_at_const(
                &function->blocks, predecessor_index
            );
            bool enters = predecessor != NULL
                && ((predecessor->terminator.kind == MMAY_MIR_GOTO
                        && predecessor->terminator.target == join_id)
                    || (predecessor->terminator.kind == MMAY_MIR_BRANCH
                        && (predecessor->terminator.target == join_id
                            || predecessor->terminator.otherwise == join_id)));
            if (!enters || predecessor_index == comparison_block_index) {
                continue;
            }
            if (!mmay_native_c_block_assigns_true(
                    predecessor, join->terminator.value
                )) {
                all_other_true = false;
                break;
            }
        }
        if (!all_other_true || safe_when_true) continue;
        safe = join->terminator.otherwise;
        if (mmay_native_c_edge_dominates(
                function, join_id, safe, target
            ) && mmay_native_c_path_preserves_registers(
                function, safe, target, target_instruction, join_id,
                index_origin, count_origin
            )) return true;
    }
    return false;
}

static bool mmay_native_c_guard_proves(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayNativeCBound bound,
    MarimayMirRegister index_origin,
    MarimayMirRegister count_origin
) {
    MarimayMirBlockId target = MMAY_MIR_NO_BLOCK;
    size_t target_instruction = 0;
    return mmay_native_c_instruction_location(
            function, instruction, &target, &target_instruction
        ) && (mmay_native_c_direct_guard_proves(
            function, target, target_instruction, bound,
            index_origin, count_origin
        ) || mmay_native_c_short_circuit_guard_proves(
            function, target, target_instruction, bound,
            index_origin, count_origin
        ) || mmay_native_c_joined_guard_proves(
            function, target, target_instruction, bound,
            index_origin, count_origin
        ));
}

bool mmay_native_c_array_filled_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register
) {
    MarimayMirRegister count = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister count_origin = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index_origin = MMAY_MIR_NO_REGISTER;
    bool filled = mmay_native_c_array_fill_count(
        function, instruction, array_register, &count
    );
    bool origins = filled && mmay_native_c_scalar_origin(
        function, count, 0, &count_origin
    ) && mmay_native_c_scalar_origin(
        function, index_register, 0, &index_origin
    );
    bool lower = origins && mmay_native_c_guard_proves(
        function, instruction, MMAY_NATIVE_C_BOUND_LOWER,
        index_origin, count_origin
    );
    bool upper = origins && mmay_native_c_guard_proves(
        function, instruction, MMAY_NATIVE_C_BOUND_UPPER,
        index_origin, count_origin
    );
    return filled && origins && lower && upper;
}

bool mmay_native_c_array_guarded_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register
) {
    MarimayMirRegister array_origin = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister index_origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    if (function == NULL || instruction == NULL
        || !mmay_native_c_array_register_origin(
            function, array_register, &array_origin
        ) || !mmay_native_c_scalar_origin(
            function, index_register, 0, &index_origin
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
            MarimayMirRegister length_array_origin = MMAY_MIR_NO_REGISTER;
            MarimayMirRegister count_origin = MMAY_MIR_NO_REGISTER;
            if (length == NULL || length->kind != MMAY_MIR_ARRAY_LENGTH
                || !mmay_native_c_array_register_origin(
                    function, length->first, &length_array_origin
                ) || length_array_origin != array_origin
                || !mmay_native_c_scalar_origin(
                    function, length->target, 0, &count_origin
                )) continue;
            if (mmay_native_c_guard_proves(
                    function, instruction, MMAY_NATIVE_C_BOUND_LOWER,
                    index_origin, count_origin
                ) && mmay_native_c_guard_proves(
                    function, instruction, MMAY_NATIVE_C_BOUND_UPPER,
                    index_origin, count_origin
                )) return true;
        }
    }
    return false;
}
