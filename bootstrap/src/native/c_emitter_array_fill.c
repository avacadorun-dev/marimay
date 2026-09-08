#include "c_emitter_internal.h"

#include <string.h>

static bool mmay_native_c_fill_successor(
    MarimayMirBlockId successor,
    MarimayMirBlockId avoid_first,
    MarimayMirBlockId avoid_second,
    size_t block_count,
    unsigned char *visited,
    MarimayMirBlockId *queue,
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

static bool mmay_native_c_fill_reachable(
    const MarimayMirFunction *function,
    MarimayMirBlockId start,
    MarimayMirBlockId target,
    MarimayMirBlockId avoid_first,
    MarimayMirBlockId avoid_second,
    bool *result
) {
    unsigned char *visited = NULL;
    MarimayMirBlockId *queue = NULL;
    size_t queue_bytes = 0, head = 0, tail = 0;
    size_t block_count = function != NULL ? function->blocks.length : 0;
    bool reachable = false, valid = false;
    if (result == NULL || start >= block_count || target >= block_count
        || !mmay_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = mmay_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    if (start != avoid_first && start != avoid_second) {
        visited[start] = 1;
        queue[tail++] = start;
    }
    while (head < tail && visited[target] == 0) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, queue[head++]
        );
        if (block == NULL) goto cleanup;
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            if (!mmay_native_c_fill_successor(
                    block->terminator.target, avoid_first, avoid_second,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            if (!mmay_native_c_fill_successor(
                    block->terminator.target, avoid_first, avoid_second,
                    block_count, visited, queue, &tail
                ) || !mmay_native_c_fill_successor(
                    block->terminator.otherwise, avoid_first, avoid_second,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        }
    }
    reachable = visited[target] != 0;
    valid = true;
cleanup:
    mmay_deallocate(&function->allocator, queue, queue_bytes);
    mmay_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    if (valid) *result = reachable;
    return valid;
}

static bool mmay_native_c_fill_dominates(
    const MarimayMirFunction *function,
    MarimayMirBlockId candidate,
    MarimayMirBlockId target
) {
    bool reachable = false;
    return candidate == target || (
        mmay_native_c_fill_reachable(
            function, 0, target, candidate, MMAY_MIR_NO_BLOCK,
            &reachable
        ) && !reachable
    );
}

static bool mmay_native_c_fill_instruction_location(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirBlockId *result
) {
    size_t block_index = 0;
    if (function == NULL || instruction == NULL || result == NULL) return false;
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
            if (mmay_vector_at_const(
                    &block->instructions, instruction_index
                ) == instruction) {
                *result = (MarimayMirBlockId)block_index;
                return true;
            }
        }
    }
    return false;
}

static bool mmay_native_c_fill_constant(
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
            int64_t candidate = 0, source = 0;
            if (instruction == NULL) return false;
            if (instruction->target != register_id) continue;
            if (instruction->kind == MMAY_MIR_CONSTANT_I64) {
                candidate = instruction->integer;
            } else if (instruction->kind == MMAY_MIR_COPY) {
                if (!mmay_native_c_fill_constant(
                        function, instruction->first, depth + 1, &candidate
                    )) return false;
            } else if (instruction->kind == MMAY_MIR_NEGATE_CHECKED_I64) {
                if (!mmay_native_c_fill_constant(
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

static const MarimayMirInstruction *mmay_native_c_fill_condition(
    const MarimayMirBlock *block
) {
    size_t instruction_index = 0;
    const MarimayMirInstruction *result = NULL;
    if (block == NULL || block->terminator.kind != MMAY_MIR_BRANCH) return NULL;
    for (instruction_index = 0;
        instruction_index < block->instructions.length;
        instruction_index += 1) {
        const MarimayMirInstruction *instruction = mmay_vector_at_const(
            &block->instructions, instruction_index
        );
        if (instruction != NULL
            && instruction->target == block->terminator.value) {
            result = instruction;
        }
    }
    return result != NULL && result->kind == MMAY_MIR_LESS_I64
        ? result : NULL;
}

static bool mmay_native_c_fill_step(
    const MarimayMirFunction *function,
    MarimayMirBlockId condition,
    MarimayMirBlockId push_block,
    MarimayMirRegister index,
    MarimayMirBlockId exit,
    MarimayMirBlockId *result
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t copy_index = 0;
        bool reachable = false;
        if (block == NULL || block->terminator.kind != MMAY_MIR_GOTO
            || block->terminator.target != condition
            || !mmay_native_c_fill_reachable(
                function, push_block, (MarimayMirBlockId)block_index,
                condition, exit, &reachable
            ) || !reachable) continue;
        for (copy_index = 0; copy_index < block->instructions.length;
            copy_index += 1) {
            const MarimayMirInstruction *copy = mmay_vector_at_const(
                &block->instructions, copy_index
            );
            size_t add_index = 0;
            if (copy == NULL || copy->kind != MMAY_MIR_COPY
                || copy->target != index) continue;
            for (add_index = 0; add_index < block->instructions.length;
                add_index += 1) {
                const MarimayMirInstruction *add = mmay_vector_at_const(
                    &block->instructions, add_index
                );
                int64_t one = 0;
                if (add != NULL && add->target == copy->first
                    && (add->kind == MMAY_MIR_ADD_CHECKED_I64
                        || add->kind == MMAY_MIR_ADD_PROVEN_I64)
                    && add->first == index
                    && mmay_native_c_fill_constant(
                        function, add->second, 0, &one
                    ) && one == 1) {
                    *result = (MarimayMirBlockId)block_index;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool mmay_native_c_fill_region_instruction(
    const MarimayMirInstruction *instruction,
    const MarimayMirInstruction *push
) {
    if (instruction == push) return true;
    switch (instruction->kind) {
        case MMAY_MIR_CONSTANT_I64:
        case MMAY_MIR_CONSTANT_INTEGER:
        case MMAY_MIR_CONSTANT_BOOL:
        case MMAY_MIR_COPY:
        case MMAY_MIR_NEGATE_CHECKED_I64:
        case MMAY_MIR_EQUAL_I64:
        case MMAY_MIR_ARRAY_LENGTH:
        case MMAY_MIR_ENUM_NEW:
            return true;
        default:
            return false;
    }
}

static bool mmay_native_c_fill_region_valid(
    const MarimayMirFunction *function,
    MarimayMirBlockId start,
    MarimayMirBlockId step,
    MarimayMirBlockId condition,
    MarimayMirBlockId exit,
    const MarimayMirInstruction *push
) {
    unsigned char *visited = NULL;
    MarimayMirBlockId *queue = NULL;
    size_t queue_bytes = 0, head = 0, tail = 0, push_count = 0;
    size_t block_count = function->blocks.length;
    bool valid = false;
    if (!mmay_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = mmay_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    visited[start] = 1;
    queue[tail++] = start;
    while (head < tail) {
        MarimayMirBlockId block_id = queue[head++];
        const MarimayMirBlock *block = mmay_vector_at_const(
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL || !mmay_native_c_fill_region_instruction(
                    instruction, push
                )) goto cleanup;
            if (instruction == push) push_count += 1;
        }
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            if (block->terminator.target == condition
                || block->terminator.target == exit) goto cleanup;
            if (!mmay_native_c_fill_successor(
                    block->terminator.target,
                    MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            if (block->terminator.target == condition
                || block->terminator.target == exit
                || block->terminator.otherwise == condition
                || block->terminator.otherwise == exit) goto cleanup;
            if (!mmay_native_c_fill_successor(
                    block->terminator.target,
                    MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK,
                    block_count, visited, queue, &tail
                ) || !mmay_native_c_fill_successor(
                    block->terminator.otherwise,
                    MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK,
                    block_count, visited, queue, &tail
                )) goto cleanup;
        } else {
            goto cleanup;
        }
    }
    valid = visited[step] != 0 && push_count == 1;
cleanup:
    mmay_deallocate(&function->allocator, queue, queue_bytes);
    mmay_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    return valid;
}

static bool mmay_native_c_fill_empty_origin(
    const MarimayMirFunction *function,
    MarimayMirRegister origin,
    MarimayMirBlockId condition
) {
    size_t block_index = 0;
    const MarimayMirInstruction *definition = NULL;
    MarimayMirBlockId definition_block = MMAY_MIR_NO_BLOCK;
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
            if (instruction != NULL && instruction->target == origin) {
                if (definition != NULL) return false;
                definition = instruction;
                definition_block = (MarimayMirBlockId)block_index;
            }
        }
    }
    if (definition == NULL || definition->kind != MMAY_MIR_ARRAY_NEW
        || !mmay_native_c_fill_dominates(
            function, definition_block, condition
        )) return false;
    {
        const MarimayMirArrayConstruction *construction =
            mmay_native_c_array_construction(
                function, definition->array_construction
            );
        return construction != NULL && construction->argument_count == 0;
    }
}

static bool mmay_native_c_fill_only_size_mutation(
    const MarimayMirFunction *function,
    MarimayMirRegister origin,
    const MarimayMirInstruction *push
) {
    size_t block_index = 0;
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
            if (instruction->kind != MMAY_MIR_ARRAY_PUSH
                && instruction->kind != MMAY_MIR_ARRAY_POP
                && instruction->kind != MMAY_MIR_ARRAY_REMOVE_AT
                && instruction->kind != MMAY_MIR_ARRAY_TRUNCATE
                && instruction->kind != MMAY_MIR_ARRAY_CLEAR
                && instruction->kind != MMAY_MIR_ARRAY_RESERVE) continue;
            if (!mmay_native_c_array_register_origin(
                    function, instruction->first, &candidate
                )) return false;
            if (candidate == origin && instruction != push) return false;
        }
    }
    return true;
}

bool mmay_native_c_array_fill_plan(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister *count,
    MarimayMirRegister *index,
    uint64_t *bits,
    MarimayMirBlockId *exit_block
) {
    MarimayMirBlockId push_block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
    int64_t value = 0;
    size_t condition_index = 0;
    const MarimayMirArray *array = NULL;
    if (function == NULL || instruction == NULL || count == NULL
        || index == NULL || bits == NULL || function->calls.length != 0
        || instruction->kind != MMAY_MIR_ARRAY_PUSH
        || !mmay_native_c_fill_instruction_location(
            function, instruction, &push_block
        ) || !mmay_native_c_array_register_origin(
            function, instruction->first, &origin
        ) || !mmay_native_c_fill_constant(
            function, instruction->second, 0, &value
        )) return false;
    array = mmay_vector_at_const(&function->arrays, instruction->array);
    if (array == NULL || array->element_type != MMAY_TYPE_I64) return false;
    for (condition_index = 0;
        condition_index < function->blocks.length;
        condition_index += 1) {
        const MarimayMirBlock *condition_block = mmay_vector_at_const(
            &function->blocks, condition_index
        );
        const MarimayMirInstruction *condition =
            mmay_native_c_fill_condition(condition_block);
        MarimayMirBlockId step = MMAY_MIR_NO_BLOCK;
        int64_t zero = 1;
        size_t definition_count = 0, block_index = 0;
        MarimayMirBlockId initial_block = MMAY_MIR_NO_BLOCK;
        bool reaches_push = false;
        if (condition == NULL || !mmay_native_c_fill_reachable(
                function, condition_block->terminator.target, push_block,
                (MarimayMirBlockId)condition_index,
                condition_block->terminator.otherwise, &reaches_push
            ) || !reaches_push || !mmay_native_c_fill_step(
                function, (MarimayMirBlockId)condition_index, push_block,
                condition->first, condition_block->terminator.otherwise, &step
            )) continue;
        for (block_index = 0; block_index < function->blocks.length;
            block_index += 1) {
            const MarimayMirBlock *block = mmay_vector_at_const(
                &function->blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0;
                block != NULL && instruction_index < block->instructions.length;
                instruction_index += 1) {
                const MarimayMirInstruction *definition = mmay_vector_at_const(
                    &block->instructions, instruction_index
                );
                if (definition != NULL
                    && definition->target == condition->first
                    && !(block_index == step
                        && definition->kind == MMAY_MIR_COPY)) {
                    if (definition->kind != MMAY_MIR_COPY
                        || !mmay_native_c_fill_constant(
                            function, definition->first, 0, &zero
                        ) || zero != 0) {
                        definition_count = 2;
                        break;
                    }
                    definition_count += 1;
                    initial_block = (MarimayMirBlockId)block_index;
                }
            }
        }
        if (definition_count != 1
            || !mmay_native_c_fill_dominates(
                function, initial_block, (MarimayMirBlockId)condition_index
            ) || !mmay_native_c_fill_region_valid(
                function, condition_block->terminator.target, step,
                (MarimayMirBlockId)condition_index,
                condition_block->terminator.otherwise, instruction
            ) || !mmay_native_c_fill_empty_origin(
                function, origin, (MarimayMirBlockId)condition_index
            ) || !mmay_native_c_fill_only_size_mutation(
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

bool mmay_native_c_array_fill_count(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister *count
) {
    MarimayMirRegister access_origin = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId access_block = MMAY_MIR_NO_BLOCK;
    size_t block_index = 0;
    if (function == NULL || instruction == NULL || count == NULL
        || !mmay_native_c_array_register_origin(
            function, array_register, &access_origin
        ) || !mmay_native_c_fill_instruction_location(
            function, instruction, &access_block
        )) return false;
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
            const MarimayMirInstruction *candidate = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayMirRegister candidate_origin = MMAY_MIR_NO_REGISTER;
            MarimayMirRegister fill_count = MMAY_MIR_NO_REGISTER;
            MarimayMirRegister fill_index = MMAY_MIR_NO_REGISTER;
            MarimayMirBlockId exit_block = MMAY_MIR_NO_BLOCK;
            uint64_t fill_bits = 0;
            if (candidate == NULL || candidate->kind != MMAY_MIR_ARRAY_PUSH
                || !mmay_native_c_array_register_origin(
                    function, candidate->first, &candidate_origin
                ) || candidate_origin != access_origin
                || !mmay_native_c_array_fill_plan(
                    function, candidate, &fill_count, &fill_index,
                    &fill_bits, &exit_block
                ) || !mmay_native_c_fill_dominates(
                    function, exit_block, access_block
                )) continue;
            *count = fill_count;
            return true;
        }
    }
    return false;
}
