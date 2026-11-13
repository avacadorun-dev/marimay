#include "c_emitter_internal.h"

#include <string.h>

static size_t mmay_native_c_dense_scalar_stride(MarimayTypeId type) {
    if (type == MMAY_TYPE_BOOL || type == MMAY_TYPE_I8
        || type == MMAY_TYPE_U8) return sizeof(uint8_t);
    if (type == MMAY_TYPE_I16 || type == MMAY_TYPE_U16) {
        return sizeof(uint16_t);
    }
    /* `rune` is dense RAW32 with stride 4, exactly like `u32`; the scalar read
     * and store emitters already convert it, so excluding it only forced rune
     * element access through the validated runtime view call. */
    if (type == MMAY_TYPE_I32 || type == MMAY_TYPE_U32
        || type == MMAY_TYPE_RUNE
        || type == MMAY_TYPE_F32) return sizeof(uint32_t);
    if (type == MMAY_TYPE_I64 || type == MMAY_TYPE_U64
        || type == MMAY_TYPE_F64) return sizeof(uint64_t);
    return 0;
}

bool mmay_native_c_batch_scalar_mutation(const MarimayMirFunction *function) {
    MarimayMirRegister mutation_origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    bool saw_set = false;
    if (function == NULL) return false;
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
            MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind == MMAY_MIR_CALL
                || instruction->kind == MMAY_MIR_CALL_INTERFACE) return false;
            if (instruction->kind == MMAY_MIR_ARRAY_SET
                || instruction->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS) {
                const MarimayMirArray *array = mmay_vector_at_const(
                    &function->arrays, instruction->array
                );
                if (array == NULL || mmay_native_c_dense_scalar_stride(
                        array->element_type
                    ) == 0 || !mmay_native_c_array_register_origin(
                        function, instruction->first, &origin
                    )) return false;
                if (mutation_origin != MMAY_MIR_NO_REGISTER
                    && mutation_origin != origin) return false;
                mutation_origin = origin;
                saw_set = true;
            }
        }
    }
    for (block_index = 0; saw_set && block_index < function->blocks.length;
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
            MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind != MMAY_MIR_ARRAY_GENERATION
                && instruction->kind
                    != MMAY_MIR_ARRAY_REQUIRE_GENERATION) continue;
            if (!mmay_native_c_array_register_origin(
                    function, instruction->first, &origin
                ) || origin == mutation_origin) return false;
        }
    }
    return saw_set;
}

static bool mmay_native_c_block_push_successor(
    MarimayMirBlockId successor,
    MarimayMirBlockId excluded,
    size_t block_count,
    unsigned char *visited,
    MarimayMirBlockId *queue,
    size_t *tail
) {
    if (successor == MMAY_MIR_NO_BLOCK || successor == excluded) return true;
    if (successor >= block_count || visited[successor] != 0) {
        return successor < block_count;
    }
    visited[successor] = 1;
    queue[*tail] = successor;
    *tail += 1;
    return true;
}

static bool mmay_native_c_block_dominates(
    const MarimayMirFunction *function,
    MarimayMirBlockId candidate,
    MarimayMirBlockId target
) {
    unsigned char *visited = NULL;
    MarimayMirBlockId *queue = NULL;
    size_t queue_bytes = 0;
    size_t head = 0, tail = 0, block_count = 0;
    bool dominates = false;
    if (function == NULL || target >= function->blocks.length
        || candidate >= function->blocks.length) return false;
    if (candidate == target) return true;
    block_count = function->blocks.length;
    if (!mmay_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = mmay_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = mmay_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    if (candidate != 0) {
        visited[0] = 1;
        queue[tail] = 0;
        tail += 1;
    }
    while (head < tail) {
        MarimayMirBlockId block_id = queue[head];
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_id
        );
        head += 1;
        if (block == NULL) goto cleanup;
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            if (!mmay_native_c_block_push_successor(
                    block->terminator.target, candidate, block_count,
                    visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            if (!mmay_native_c_block_push_successor(
                    block->terminator.target, candidate, block_count,
                    visited, queue, &tail
                ) || !mmay_native_c_block_push_successor(
                    block->terminator.otherwise, candidate, block_count,
                    visited, queue, &tail
                )) goto cleanup;
        }
    }
    dominates = visited[target] == 0;
cleanup:
    mmay_deallocate(&function->allocator, queue, queue_bytes);
    mmay_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    return dominates;
}

static bool mmay_native_c_array_length_cached(
    const MarimayMirFunction *function,
    const MarimayMirBlock *block,
    const MarimayMirInstruction *instruction
) {
    MarimayMirBlockId block_id = MMAY_MIR_NO_BLOCK;
    size_t candidate_index = 0, index = 0;
    if (function == NULL || block == NULL || instruction == NULL
        || !mmay_native_c_array_register_unmodified(
            function, instruction->first
        )) return false;
    for (index = 0; index < function->blocks.length; index += 1) {
        if (mmay_vector_at_const(&function->blocks, index) == block) {
            block_id = (MarimayMirBlockId)index;
            break;
        }
    }
    if (block_id == MMAY_MIR_NO_BLOCK) return false;
    for (candidate_index = 0;
        candidate_index < function->blocks.length;
        candidate_index += 1) {
        const MarimayMirBlock *candidate = mmay_vector_at_const(
            &function->blocks, candidate_index
        );
        size_t instruction_index = 0;
        if (candidate == NULL || candidate == block
            || !mmay_native_c_block_dominates(
                function, (MarimayMirBlockId)candidate_index, block_id
            )) continue;
        for (instruction_index = 0;
            instruction_index < candidate->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *previous = mmay_vector_at_const(
                &candidate->instructions, instruction_index
            );
            if (previous != NULL
                && previous->kind == MMAY_MIR_ARRAY_LENGTH
                && previous->target == instruction->target
                && previous->first == instruction->first
                && previous->array == instruction->array) return true;
        }
    }
    return false;
}

static bool mmay_native_c_emit_scalar_from_bits(
    MarimayNativeCBuffer *buffer,
    MarimayTypeId type,
    MarimayMirRegister target
) {
    if (type == MMAY_TYPE_I64) {
        return mmay_native_c_write(
            buffer,
            " mmay_g_v_%u = mmay_i64_from_twos_complement_fast(mmay_g_bits);",
            target
        );
    }
    if (type == MMAY_TYPE_BOOL) {
        return mmay_native_c_write(
            buffer, " mmay_g_v_%u = mmay_g_bits != 0;", target
        );
    }
    if (type == MMAY_TYPE_U64 || type == MMAY_TYPE_F64) {
        return mmay_native_c_write(
            buffer, " mmay_g_v_%u = mmay_g_bits;", target
        );
    }
    if (type == MMAY_TYPE_F32) {
        return mmay_native_c_write(
            buffer, " mmay_g_v_%u = (uint32_t)mmay_g_bits;", target
        );
    }
    return mmay_native_c_write(
            buffer, " { MarimayManagedCell mmay_g_cell = {0}; mmay_g_cell.f64_bits = mmay_g_bits; "
        ) && mmay_native_c_write_from_managed_cell(buffer, type, target)
        && mmay_native_c_write(buffer, "; }");
}

static const MarimayMirInstruction *mmay_native_c_array_proof(
    const MarimayMirBlock *block,
    size_t instruction_index,
    MarimayMirRegister carrier,
    size_t *proof_index
) {
    size_t cursor = instruction_index;
    if (proof_index != NULL) {
        *proof_index = SIZE_MAX;
    }
    while (block != NULL && cursor != 0) {
        const MarimayMirInstruction *candidate = NULL;
        cursor -= 1;
        candidate = mmay_vector_at_const(&block->instructions, cursor);
        if (candidate == NULL || candidate->target != carrier) {
            continue;
        }
        if ((candidate->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                || candidate->kind == MMAY_MIR_ARRAY_ASSUME_INDEX)
            && proof_index != NULL) {
            *proof_index = cursor;
            return candidate;
        }
        return NULL;
    }
    return NULL;
}

static bool mmay_native_c_array_require_is_fused(
    const MarimayMirBlock *block,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirInstruction *consumer = block != NULL
        ? mmay_vector_at_const(
            &block->instructions, instruction_index + 1
        ) : NULL;
    return instruction != NULL && consumer != NULL
        && (consumer->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS
            || consumer->kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS)
        && consumer->first == instruction->target
        && consumer->second == instruction->second;
}

static bool mmay_native_c_array_index_proven(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister array_register,
    MarimayMirRegister index_register
) {
    return mmay_native_c_array_filled_index_proven(
            function, instruction, array_register, index_register
        ) || mmay_native_c_array_relational_index_proven(
            function, functions, function_count, instruction,
            array_register, index_register
        ) || mmay_native_c_array_loaded_index_proven(
            function, functions, function_count, instruction,
            array_register, index_register
        );
}

static bool mmay_native_c_emit_array_bounds_failure(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    return mmay_native_c_write(
            buffer,
            " if (mmay_g_m == MMAY_MANAGED_BOUNDS) { mmay_g_s = mmay_g_begin_bounds(mmay_g_p, mmay_g_v_%u, mmay_g_length, ",
            instruction->second
        ) && mmay_native_c_symbol_data(buffer, function->symbol)
        && mmay_native_c_write(
            buffer,
            ", %zu, %zu); goto mmay_g_cleanup; } if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }",
            instruction->span.start,
            instruction->span.end
        );
}

static bool mmay_native_c_emit_array_view_bind(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    const MarimayMirArray *array
) {
    if (mmay_native_c_array_view_known(function, register_id)) return true;
    return mmay_native_c_write(
        buffer,
        " if (mmay_g_av_%u.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, %s, &mmay_g_av_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }",
        register_id, register_id,
        mmay_native_c_array_descriptor(function, array->type), register_id
    );
}

static bool mmay_native_c_emit_direct_bounds_check(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirRegister index
) {
    return mmay_native_c_write(
            buffer,
            " if (mmay_g_v_%u < 0 || (uint64_t)mmay_g_v_%u >= (uint64_t)mmay_g_length) { mmay_g_s = mmay_g_begin_bounds(mmay_g_p, mmay_g_v_%u, mmay_g_length, ",
            index, index, index
        ) && mmay_native_c_symbol_data(buffer, function->symbol)
        && mmay_native_c_write(
            buffer,
            ", %zu, %zu); goto mmay_g_cleanup; }",
            instruction->span.start,
            instruction->span.end
        );
}

static bool mmay_native_c_emit_array_get(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirInstruction *instruction,
    const MarimayMirInstruction *proof,
    bool checked
) {
    const MarimayMirArray *array = mmay_vector_at_const(
        &function->arrays, instruction->array
    );
    const MarimayMirInstruction *panic_site = checked && proof != NULL
        ? proof : instruction;
    size_t scalar_stride = array != NULL
        ? mmay_native_c_dense_scalar_stride(array->element_type) : 0;
    MarimayMirRegister source_origin = MMAY_MIR_NO_REGISTER;
    bool cell = array != NULL
        && (array->element_type == MMAY_TYPE_STRING
            || array->element_type >= MMAY_TYPE_FIRST_DYNAMIC);
    MarimayNativeCManagedLayout target_layout;
    bool array_target = cell
        && mmay_native_c_register_array(function, instruction->target) != NULL;
    bool record_target = cell && mmay_native_c_find_managed_layout(
            functions, function_count, array->element_type, &target_layout
        ) && target_layout.kind == MMAY_NATIVE_C_MANAGED_RECORD;
    const MarimayMirArray *target_array = array_target
        ? mmay_native_c_register_array(function, instruction->target) : NULL;
    bool prewarmed_children = (array_target || record_target)
        && mmay_native_c_array_register_origin(
            function, instruction->first, &source_origin
        ) && source_origin < function->parameter_count
        && mmay_native_c_array_children_parameter(
            function, functions, function_count, source_origin
        );
    /* A mutable parent may retain a stale cache from a read-only callee.
     * Resolve the selected, validated cell directly; refreshing every child
     * after each mutation turns growing record/nested-array queues quadratic. */
    bool snapshot_array = prewarmed_children && target_array != NULL
        && mmay_native_c_dense_scalar_stride(target_array->element_type) != 0
        && mmay_native_c_array_register_unmodified(
            function, instruction->target
        );
    if (scalar_stride != 0) {
        if (!mmay_native_c_write(
                buffer,
                "  { uint64_t mmay_g_bits = 0;"
            ) || !mmay_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            )) {
            return false;
        }
        if (checked
            && (!mmay_native_c_write(
                    buffer,
                    " size_t mmay_g_length = mmay_g_av_%u.object->length;",
                    instruction->first
                ) || !mmay_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!mmay_native_c_write(
                buffer,
                " mmay_g_bits = mmay_managed_array_generated_get_scalar(mmay_g_av_%u.object, (size_t)mmay_g_v_%u, %zu);",
                instruction->first, instruction->second, scalar_stride
            )) {
            return false;
        }
        return mmay_native_c_emit_scalar_from_bits(
                buffer, array->element_type, instruction->target
            ) && mmay_native_c_write(buffer, " }\n");
    }
    if (cell) {
        if (!mmay_native_c_write(
                buffer, "  { MarimayManagedCell mmay_g_cell;"
            ) || !mmay_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            )) {
            return false;
        }
        if (checked
            && (!mmay_native_c_write(
                    buffer,
                    " size_t mmay_g_length = mmay_g_av_%u.object->length;",
                    instruction->first
                ) || !mmay_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!mmay_native_c_write(
                buffer,
                " memcpy(&mmay_g_cell, mmay_g_av_%u.object->storage + (size_t)mmay_g_v_%u * sizeof(mmay_g_cell), sizeof(mmay_g_cell)); ",
                instruction->first, instruction->second
            ) || !(array->element_type == MMAY_TYPE_STRING
                ? mmay_native_c_write(buffer,
                    "mmay_g_v_%u.managed = mmay_g_cell.reference; mmay_managed_string_generated_bytes(&mmay_g_x->heap, mmay_g_cell.reference, &mmay_g_v_%u.data, &mmay_g_v_%u.length)",
                    instruction->target, instruction->target, instruction->target)
                : mmay_native_c_write_from_managed_cell(
                    buffer, array->element_type, instruction->target))
            || (array_target && !prewarmed_children
                && !mmay_native_c_write(
                    buffer,
                    "; mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}; mmay_g_av_%u.object = mmay_managed_array_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)",
                    instruction->target, instruction->target
            )) || (array_target && prewarmed_children
                && !mmay_native_c_write(
                    buffer,
                    snapshot_array
                        ? "; mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}; mmay_g_av_%u.object = ((void)mmay_g_av_%u.object, &mmay_g_av_%u.object->generated_children->entries[(size_t)mmay_g_v_%u].array_snapshot)"
                        : "; mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}; mmay_g_av_%u.object = ((void)mmay_g_av_%u.object, (MarimayManagedArray *)mmay_g_av_%u.object->generated_children->entries[(size_t)mmay_g_v_%u].object)",
                    instruction->target, instruction->target,
                    instruction->first, instruction->first,
                    instruction->second
            )) || (record_target && !prewarmed_children
                && !mmay_native_c_write(
                    buffer,
                    "; mmay_g_rv_%u.reference = mmay_g_cell.reference; mmay_g_rv_%u.object = mmay_managed_record_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)",
                    instruction->target, instruction->target
            )) || (record_target && prewarmed_children
                && !mmay_native_c_write(
                    buffer,
                    "; mmay_g_rv_%u.reference = mmay_g_cell.reference; mmay_g_rv_%u.object = ((void)mmay_g_av_%u.object, (MarimayManagedRecord *)mmay_g_av_%u.object->generated_children->entries[(size_t)mmay_g_v_%u].object)",
                    instruction->target, instruction->target,
                    instruction->first, instruction->first,
                    instruction->second
                ))) {
            return false;
        }
        return mmay_native_c_write(buffer, "; }\n");
    }
    if (array == NULL || !mmay_native_c_write(
            buffer,
            "  { MarimayManagedCell mmay_g_cell; size_t mmay_g_length = 0; MarimayManagedArrayView mmay_g_checked;"
        ) || !mmay_native_c_emit_array_view_bind(
            buffer, function, instruction->first, array
        ) || !mmay_native_c_write(
            buffer,
            " mmay_g_checked = mmay_managed_array_generated_expand(&mmay_g_av_%u, mmay_g_v_%u); mmay_g_m = mmay_managed_array_view_get(&mmay_g_checked, mmay_g_v_%u, %s, &mmay_g_cell, &mmay_g_length);",
            instruction->first, instruction->first,
            instruction->second,
            checked ? "MMAY_MANAGED_ARRAY_ACCESS_CHECKED"
                    : "MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS"
        ) || !mmay_native_c_emit_array_bounds_failure(
            buffer, function, panic_site
        ) || !mmay_native_c_write(buffer, " ")
        || !mmay_native_c_write_from_managed_cell(
            buffer, array->element_type, instruction->target
        ) || (mmay_native_c_register_array(function, instruction->target)
            != NULL && !mmay_native_c_write(
                buffer,
                "; mmay_g_av_%u.object = mmay_managed_array_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)",
                instruction->target
            ))) {
        return false;
    }
    return mmay_native_c_write(buffer, "; }\n");
}

static bool mmay_native_c_emit_array_set(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirInstruction *proof,
    bool checked
) {
    const MarimayMirArray *array = mmay_vector_at_const(
        &function->arrays, instruction->array
    );
    const MarimayMirInstruction *panic_site = checked && proof != NULL
        ? proof : instruction;
    size_t scalar_stride = array != NULL
        ? mmay_native_c_dense_scalar_stride(array->element_type) : 0;
    if (scalar_stride != 0) {
        if (!mmay_native_c_write(
                buffer, "  { uint64_t mmay_g_bits = (uint64_t)mmay_g_v_%u;",
                instruction->third
            ) || !mmay_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            )) {
            return false;
        }
        if (checked
            && (!mmay_native_c_write(
                    buffer,
                    " size_t mmay_g_length = mmay_g_av_%u.object->length;",
                    instruction->first
                ) || !mmay_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!(mmay_native_c_batch_scalar_mutation(function)
                ? mmay_native_c_write(
                    buffer,
                    " if (!mmay_g_array_mutated) { if (mmay_g_av_%u.object->mutation_generation == UINT64_MAX) { mmay_g_s = mmay_g_managed_status(MMAY_MANAGED_LIMIT_REACHED); goto mmay_g_cleanup; } mmay_g_av_%u.object->mutation_generation += UINT64_C(1); mmay_g_array_mutated = true; } mmay_managed_array_generated_set_scalar_untracked(mmay_g_av_%u.object, (size_t)mmay_g_v_%u, %zu, mmay_g_bits);",
                    instruction->first, instruction->first,
                    instruction->first, instruction->second, scalar_stride
                ) : mmay_native_c_write(
                    buffer,
                    " if (!mmay_managed_array_generated_set_scalar(mmay_g_av_%u.object, (size_t)mmay_g_v_%u, %zu, mmay_g_bits)) { mmay_g_s = mmay_g_managed_status(MMAY_MANAGED_LIMIT_REACHED); goto mmay_g_cleanup; }",
                    instruction->first, instruction->second, scalar_stride
                ))) {
            return false;
        }
        return mmay_native_c_write(
            buffer,
            " mmay_g_v_%u = mmay_g_v_%u; }\n",
            instruction->target, instruction->third
        );
    }
    if (array == NULL || !mmay_native_c_write(
            buffer,
            "  { MarimayManagedCell mmay_g_cell; size_t mmay_g_length = 0; MarimayManagedArrayView mmay_g_checked; memset(&mmay_g_cell, 0, sizeof(mmay_g_cell)); "
        ) || !mmay_native_c_write_to_managed_cell(
            buffer, array->element_type, instruction->third
        ) || !mmay_native_c_write(buffer, ";")
        || !mmay_native_c_emit_array_view_bind(
            buffer, function, instruction->first, array
        ) || !mmay_native_c_write(
            buffer,
            " mmay_g_checked = mmay_managed_array_generated_expand(&mmay_g_av_%u, mmay_g_v_%u); mmay_g_m = mmay_managed_array_view_set(&mmay_g_x->heap, &mmay_g_checked, mmay_g_v_%u, %s, mmay_g_cell, &mmay_g_length);",
            instruction->first, instruction->first,
            instruction->second,
            checked ? "MMAY_MANAGED_ARRAY_ACCESS_CHECKED"
                    : "MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS"
        ) || !mmay_native_c_emit_array_bounds_failure(
            buffer, function, panic_site
        )) {
        return false;
    }
    return mmay_native_c_write(
        buffer,
        " mmay_g_v_%u = mmay_g_v_%u; }\n",
        instruction->target,
        instruction->third
    );
}

bool mmay_native_c_emit_range_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirBlock *block,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirArray *array = NULL;
    const MarimayMirInstruction *proof = NULL;
    size_t proof_index = SIZE_MAX;
    bool checked = true;
    if (buffer == NULL || function == NULL || block == NULL
        || instruction == NULL) {
        return false;
    }
    array = mmay_vector_at_const(&function->arrays, instruction->array);
    if (array == NULL) {
        return false;
    }
    switch (instruction->kind) {
        case MMAY_MIR_ARRAY_LENGTH:
            if (mmay_native_c_array_length_cached(
                    function, block, instruction
                )) return mmay_native_c_write(buffer, "  (void)0;\n");
            return mmay_native_c_write(
                buffer,
                "  { size_t mmay_g_length;"
            ) && mmay_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            ) && mmay_native_c_write(
                buffer,
                " mmay_g_length = mmay_g_av_%u.object->length; if (mmay_g_length > (size_t)INT64_MAX) { mmay_g_s = 6; goto mmay_g_cleanup; } mmay_g_v_%u = (int64_t)mmay_g_length; }\n",
                instruction->first,
                instruction->target
            );
        case MMAY_MIR_ARRAY_REQUIRE_INDEX:
            if (mmay_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                )) {
                return mmay_native_c_write(
                    buffer,
                    "  mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u;\n",
                    instruction->target, instruction->first,
                    instruction->target, instruction->first
                );
            }
            if (mmay_native_c_array_require_is_fused(
                    block, instruction_index, instruction
                )) {
                return mmay_native_c_write(
                    buffer,
                    "  mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u;\n",
                    instruction->target, instruction->first,
                    instruction->target, instruction->first
                );
            }
            if (!mmay_native_c_write(
                    buffer,
                    "  { size_t mmay_g_length;"
                ) || !mmay_native_c_emit_array_view_bind(
                    buffer, function, instruction->first, array
                ) || !mmay_native_c_write(
                    buffer,
                    " mmay_g_length = mmay_g_av_%u.object->length;",
                    instruction->first
                ) || !mmay_native_c_emit_direct_bounds_check(
                    buffer, function, instruction, instruction->second
                )) {
                return false;
            }
            return mmay_native_c_write(
                buffer,
                " mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
                instruction->target, instruction->first,
                instruction->target, instruction->first
            );
        case MMAY_MIR_ARRAY_ASSUME_INDEX:
            return mmay_native_c_write(
                buffer,
                "  mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u;\n",
                instruction->target, instruction->first,
                instruction->target, instruction->first
            );
        case MMAY_MIR_ARRAY_GET:
            return mmay_native_c_emit_array_get(
                buffer, function, functions, function_count,
                instruction, NULL,
                !mmay_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                )
            );
        case MMAY_MIR_ARRAY_SET:
            return mmay_native_c_emit_array_set(
                buffer, function, instruction, NULL,
                !mmay_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                )
            );
        case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
        case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
            proof = mmay_native_c_array_proof(
                block, instruction_index, instruction->first, &proof_index
            );
            if (proof == NULL) {
                return false;
            }
            checked = proof->kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
                && proof_index + 1 == instruction_index
                && !mmay_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                );
            return instruction->kind == MMAY_MIR_ARRAY_GET_IN_BOUNDS
                ? mmay_native_c_emit_array_get(
                    buffer, function, functions, function_count,
                    instruction, proof, checked
                )
                : mmay_native_c_emit_array_set(
                    buffer, function, instruction, proof, checked
                );
        default:
            return false;
    }
}
