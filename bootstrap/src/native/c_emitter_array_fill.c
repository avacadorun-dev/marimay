#include "c_emitter_internal.h"

#include <string.h>

static bool ruda_native_c_fill_successor(
    RudaMirBlockId successor,
    RudaMirBlockId avoid_first,
    RudaMirBlockId avoid_second,
    size_t block_count,
    unsigned char *visited,
    RudaMirBlockId *queue,
    size_t *tail
) {
    if (successor == avoid_first || successor == avoid_second) return true;
    if (successor >= block_count) return false;
    if (visited[successor] != 0) return true;
    visited[successor] = 1;
    queue[*tail] = successor;
    *tail += 1;
    return true;
}

static bool ruda_native_c_fill_reachable(
    const RudaMirFunction *function,
    RudaMirBlockId start,
    RudaMirBlockId target,
    RudaMirBlockId avoid_first,
    RudaMirBlockId avoid_second,
    bool *result
) {
    unsigned char *visited = NULL;
    RudaMirBlockId *queue = NULL;
    size_t queue_bytes = 0, head = 0, tail = 0;
    size_t block_count = function != NULL ? function->blocks.length : 0;
    bool reachable = false, valid = false;
    if (result == NULL || start >= block_count || target >= block_count
        || !ruda_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = ruda_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = ruda_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    if (start != avoid_first && start != avoid_second) {
        visited[start] = 1;
        queue[tail++] = start;
    }
    while (head < tail && visited[target] == 0) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, queue[head++]
        );
        if (block == NULL) goto cleanup;
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (!ruda_native_c_fill_successor(
                    block->terminator.target, avoid_first, avoid_second,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (!ruda_native_c_fill_successor(
                    block->terminator.target, avoid_first, avoid_second,
                    block_count, visited, queue, &tail
                ) || !ruda_native_c_fill_successor(
                    block->terminator.otherwise, avoid_first, avoid_second,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        }
    }
    reachable = visited[target] != 0;
    valid = true;
cleanup:
    ruda_deallocate(&function->allocator, queue, queue_bytes);
    ruda_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    if (valid) *result = reachable;
    return valid;
}

static bool ruda_native_c_fill_dominates(
    const RudaMirFunction *function,
    RudaMirBlockId candidate,
    RudaMirBlockId target
) {
    bool reachable = false;
    return candidate == target || (
        ruda_native_c_fill_reachable(
            function, 0, target, candidate, RUDA_MIR_NO_BLOCK,
            &reachable
        ) && !reachable
    );
}

static bool ruda_native_c_fill_instruction_location(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirBlockId *result
) {
    size_t block_index = 0;
    if (function == NULL || instruction == NULL || result == NULL) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            if (ruda_vector_at_const(
                    &block->instructions, instruction_index
                ) == instruction) {
                *result = (RudaMirBlockId)block_index;
                return true;
            }
        }
    }
    return false;
}

static bool ruda_native_c_fill_constant(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
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
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            int64_t candidate = 0, source = 0;
            if (instruction == NULL) return false;
            if (instruction->target != register_id) continue;
            if (instruction->kind == RUDA_MIR_CONSTANT_I64) {
                candidate = instruction->integer;
            } else if (instruction->kind == RUDA_MIR_COPY) {
                if (!ruda_native_c_fill_constant(
                        function, instruction->first, depth + 1, &candidate
                    )) return false;
            } else if (instruction->kind == RUDA_MIR_NEGATE_CHECKED_I64) {
                if (!ruda_native_c_fill_constant(
                        function, instruction->first, depth + 1, &source
                    ) || source == INT64_MIN) return false;
                candidate = -source;
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

static const RudaMirInstruction *ruda_native_c_fill_condition(
    const RudaMirBlock *block
) {
    size_t instruction_index = 0;
    const RudaMirInstruction *result = NULL;
    if (block == NULL || block->terminator.kind != RUDA_MIR_BRANCH) return NULL;
    for (instruction_index = 0;
        instruction_index < block->instructions.length;
        instruction_index += 1) {
        const RudaMirInstruction *instruction = ruda_vector_at_const(
            &block->instructions, instruction_index
        );
        if (instruction != NULL
            && instruction->target == block->terminator.value) {
            result = instruction;
        }
    }
    return result != NULL && result->kind == RUDA_MIR_LESS_I64
        ? result : NULL;
}

static bool ruda_native_c_fill_step(
    const RudaMirFunction *function,
    RudaMirBlockId condition,
    RudaMirBlockId push_block,
    RudaMirRegister index,
    RudaMirBlockId exit,
    RudaMirBlockId *result
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t copy_index = 0;
        bool reachable = false;
        if (block == NULL || block->terminator.kind != RUDA_MIR_GOTO
            || block->terminator.target != condition
            || !ruda_native_c_fill_reachable(
                function, push_block, (RudaMirBlockId)block_index,
                condition, exit, &reachable
            ) || !reachable) continue;
        for (copy_index = 0; copy_index < block->instructions.length;
            copy_index += 1) {
            const RudaMirInstruction *copy = ruda_vector_at_const(
                &block->instructions, copy_index
            );
            size_t add_index = 0;
            if (copy == NULL || copy->kind != RUDA_MIR_COPY
                || copy->target != index) continue;
            for (add_index = 0; add_index < block->instructions.length;
                add_index += 1) {
                const RudaMirInstruction *add = ruda_vector_at_const(
                    &block->instructions, add_index
                );
                int64_t one = 0;
                if (add != NULL && add->target == copy->first
                    && (add->kind == RUDA_MIR_ADD_CHECKED_I64
                        || add->kind == RUDA_MIR_ADD_PROVEN_I64)
                    && add->first == index
                    && ruda_native_c_fill_constant(
                        function, add->second, 0, &one
                    ) && one == 1) {
                    *result = (RudaMirBlockId)block_index;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool ruda_native_c_fill_region_instruction(
    const RudaMirInstruction *instruction,
    const RudaMirInstruction *push
) {
    if (instruction == push) return true;
    switch (instruction->kind) {
        case RUDA_MIR_CONSTANT_I64:
        case RUDA_MIR_CONSTANT_INTEGER:
        case RUDA_MIR_CONSTANT_BOOL:
        case RUDA_MIR_COPY:
        case RUDA_MIR_NEGATE_CHECKED_I64:
        case RUDA_MIR_EQUAL_I64:
        case RUDA_MIR_ARRAY_LENGTH:
        case RUDA_MIR_ENUM_NEW:
            return true;
        default:
            return false;
    }
}

static bool ruda_native_c_fill_region_valid(
    const RudaMirFunction *function,
    RudaMirBlockId start,
    RudaMirBlockId step,
    RudaMirBlockId condition,
    RudaMirBlockId exit,
    const RudaMirInstruction *push
) {
    unsigned char *visited = NULL;
    RudaMirBlockId *queue = NULL;
    size_t queue_bytes = 0, head = 0, tail = 0, push_count = 0;
    size_t block_count = function->blocks.length;
    bool valid = false;
    if (!ruda_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = ruda_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = ruda_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    visited[start] = 1;
    queue[tail++] = start;
    while (head < tail) {
        RudaMirBlockId block_id = queue[head++];
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_id
        );
        size_t instruction_index = 0;
        if (block == NULL || block_id == condition || block_id == exit) {
            goto cleanup;
        }
        if (block_id == step) continue;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL || !ruda_native_c_fill_region_instruction(
                    instruction, push
                )) goto cleanup;
            if (instruction == push) push_count += 1;
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (block->terminator.target == condition
                || block->terminator.target == exit) goto cleanup;
            if (!ruda_native_c_fill_successor(
                    block->terminator.target,
                    RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (block->terminator.target == condition
                || block->terminator.target == exit
                || block->terminator.otherwise == condition
                || block->terminator.otherwise == exit) goto cleanup;
            if (!ruda_native_c_fill_successor(
                    block->terminator.target,
                    RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK,
                    block_count, visited, queue, &tail
                ) || !ruda_native_c_fill_successor(
                    block->terminator.otherwise,
                    RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else {
            goto cleanup;
        }
    }
    valid = visited[step] != 0 && push_count == 1;
cleanup:
    ruda_deallocate(&function->allocator, queue, queue_bytes);
    ruda_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    return valid;
}

static bool ruda_native_c_fill_empty_origin(
    const RudaMirFunction *function,
    RudaMirRegister origin,
    RudaMirBlockId condition
) {
    size_t block_index = 0;
    const RudaMirInstruction *definition = NULL;
    RudaMirBlockId definition_block = RUDA_MIR_NO_BLOCK;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL && instruction->target == origin) {
                if (definition != NULL) return false;
                definition = instruction;
                definition_block = (RudaMirBlockId)block_index;
            }
        }
    }
    if (definition == NULL || definition->kind != RUDA_MIR_ARRAY_NEW
        || !ruda_native_c_fill_dominates(
            function, definition_block, condition
        )) return false;
    {
        const RudaMirArrayConstruction *construction =
            ruda_native_c_array_construction(
                function, definition->array_construction
            );
        return construction != NULL && construction->argument_count == 0;
    }
}

static bool ruda_native_c_fill_only_size_mutation(
    const RudaMirFunction *function,
    RudaMirRegister origin,
    const RudaMirInstruction *push
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister candidate = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind != RUDA_MIR_ARRAY_PUSH
                && instruction->kind != RUDA_MIR_ARRAY_POP
                && instruction->kind != RUDA_MIR_ARRAY_TRUNCATE
                && instruction->kind != RUDA_MIR_ARRAY_CLEAR
                && instruction->kind != RUDA_MIR_ARRAY_RESERVE) continue;
            if (!ruda_native_c_array_register_origin(
                    function, instruction->first, &candidate
                )) return false;
            if (candidate == origin && instruction != push) return false;
        }
    }
    return true;
}

bool ruda_native_c_array_fill_plan(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirRegister *count,
    RudaMirRegister *index,
    uint64_t *bits,
    RudaMirBlockId *exit_block
) {
    RudaMirBlockId push_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
    int64_t value = 0;
    size_t condition_index = 0;
    const RudaMirArray *array = NULL;
    if (function == NULL || instruction == NULL || count == NULL
        || index == NULL || bits == NULL || function->calls.length != 0
        || instruction->kind != RUDA_MIR_ARRAY_PUSH
        || !ruda_native_c_fill_instruction_location(
            function, instruction, &push_block
        ) || !ruda_native_c_array_register_origin(
            function, instruction->first, &origin
        ) || !ruda_native_c_fill_constant(
            function, instruction->second, 0, &value
        )) return false;
    array = ruda_vector_at_const(&function->arrays, instruction->array);
    if (array == NULL || array->element_type != RUDA_TYPE_I64) return false;
    for (condition_index = 0;
        condition_index < function->blocks.length;
        condition_index += 1) {
        const RudaMirBlock *condition_block = ruda_vector_at_const(
            &function->blocks, condition_index
        );
        const RudaMirInstruction *condition =
            ruda_native_c_fill_condition(condition_block);
        RudaMirBlockId step = RUDA_MIR_NO_BLOCK;
        int64_t zero = 1;
        size_t definition_count = 0, block_index = 0;
        RudaMirBlockId initial_block = RUDA_MIR_NO_BLOCK;
        bool reaches_push = false;
        if (condition == NULL || !ruda_native_c_fill_reachable(
                function, condition_block->terminator.target, push_block,
                (RudaMirBlockId)condition_index,
                condition_block->terminator.otherwise, &reaches_push
            ) || !reaches_push || !ruda_native_c_fill_step(
                function, (RudaMirBlockId)condition_index, push_block,
                condition->first, condition_block->terminator.otherwise, &step
            )) continue;
        for (block_index = 0; block_index < function->blocks.length;
            block_index += 1) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0;
                block != NULL && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const RudaMirInstruction *definition = ruda_vector_at_const(
                    &block->instructions, instruction_index
                );
                if (definition != NULL
                    && definition->target == condition->first
                    && !(block_index == step
                        && definition->kind == RUDA_MIR_COPY)) {
                    if (definition->kind != RUDA_MIR_COPY
                        || !ruda_native_c_fill_constant(
                            function, definition->first, 0, &zero
                        ) || zero != 0) {
                        definition_count = 2;
                        break;
                    }
                    definition_count += 1;
                    initial_block = (RudaMirBlockId)block_index;
                }
            }
        }
        if (definition_count != 1
            || !ruda_native_c_fill_dominates(
                function, initial_block, (RudaMirBlockId)condition_index
            ) || !ruda_native_c_fill_region_valid(
                function, condition_block->terminator.target, step,
                (RudaMirBlockId)condition_index,
                condition_block->terminator.otherwise, instruction
            ) || !ruda_native_c_fill_empty_origin(
                function, origin, (RudaMirBlockId)condition_index
            ) || !ruda_native_c_fill_only_size_mutation(
                function, origin, instruction
            )) continue;
        *count = condition->second;
        *index = condition->first;
        *bits = (uint64_t)value;
        if (exit_block != NULL) {
            *exit_block = condition_block->terminator.otherwise;
        }
        return true;
    }
    return false;
}

bool ruda_native_c_array_fill_count(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirRegister array_register,
    RudaMirRegister *count
) {
    RudaMirRegister access_origin = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId access_block = RUDA_MIR_NO_BLOCK;
    size_t block_index = 0;
    if (function == NULL || instruction == NULL || count == NULL
        || !ruda_native_c_array_register_origin(
            function, array_register, &access_origin
        ) || !ruda_native_c_fill_instruction_location(
            function, instruction, &access_block
        )) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *candidate = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister candidate_origin = RUDA_MIR_NO_REGISTER;
            RudaMirRegister fill_count = RUDA_MIR_NO_REGISTER;
            RudaMirRegister fill_index = RUDA_MIR_NO_REGISTER;
            RudaMirBlockId exit_block = RUDA_MIR_NO_BLOCK;
            uint64_t fill_bits = 0;
            if (candidate == NULL || candidate->kind != RUDA_MIR_ARRAY_PUSH
                || !ruda_native_c_array_register_origin(
                    function, candidate->first, &candidate_origin
                ) || candidate_origin != access_origin
                || !ruda_native_c_array_fill_plan(
                    function, candidate, &fill_count, &fill_index,
                    &fill_bits, &exit_block
                ) || !ruda_native_c_fill_dominates(
                    function, exit_block, access_block
                )) continue;
            *count = fill_count;
            return true;
        }
    }
    return false;
}
