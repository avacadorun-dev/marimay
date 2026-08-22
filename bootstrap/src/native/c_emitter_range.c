#include "c_emitter_internal.h"

#include <string.h>

static size_t ruda_native_c_dense_scalar_stride(RudaTypeId type) {
    if (type == RUDA_TYPE_BOOL || type == RUDA_TYPE_I8
        || type == RUDA_TYPE_U8) return sizeof(uint8_t);
    if (type == RUDA_TYPE_I16 || type == RUDA_TYPE_U16) {
        return sizeof(uint16_t);
    }
    if (type == RUDA_TYPE_I32 || type == RUDA_TYPE_U32
        || type == RUDA_TYPE_F32) return sizeof(uint32_t);
    if (type == RUDA_TYPE_I64 || type == RUDA_TYPE_U64
        || type == RUDA_TYPE_F64) return sizeof(uint64_t);
    return 0;
}

bool ruda_native_c_batch_scalar_mutation(const RudaMirFunction *function) {
    RudaMirRegister mutation_origin = RUDA_MIR_NO_REGISTER;
    size_t block_index = 0;
    bool saw_set = false;
    if (function == NULL) return false;
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
            RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind == RUDA_MIR_CALL
                || instruction->kind == RUDA_MIR_CALL_INTERFACE) return false;
            if (instruction->kind == RUDA_MIR_ARRAY_SET
                || instruction->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS) {
                const RudaMirArray *array = ruda_vector_at_const(
                    &function->arrays, instruction->array
                );
                if (array == NULL || ruda_native_c_dense_scalar_stride(
                        array->element_type
                    ) == 0 || !ruda_native_c_array_register_origin(
                        function, instruction->first, &origin
                    )) return false;
                if (mutation_origin != RUDA_MIR_NO_REGISTER
                    && mutation_origin != origin) return false;
                mutation_origin = origin;
                saw_set = true;
            }
        }
    }
    for (block_index = 0; saw_set && block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind != RUDA_MIR_ARRAY_GENERATION
                && instruction->kind
                    != RUDA_MIR_ARRAY_REQUIRE_GENERATION) continue;
            if (!ruda_native_c_array_register_origin(
                    function, instruction->first, &origin
                ) || origin == mutation_origin) return false;
        }
    }
    return saw_set;
}

static bool ruda_native_c_block_push_successor(
    RudaMirBlockId successor,
    RudaMirBlockId excluded,
    size_t block_count,
    unsigned char *visited,
    RudaMirBlockId *queue,
    size_t *tail
) {
    if (successor == RUDA_MIR_NO_BLOCK || successor == excluded) return true;
    if (successor >= block_count || visited[successor] != 0) {
        return successor < block_count;
    }
    visited[successor] = 1;
    queue[*tail] = successor;
    *tail += 1;
    return true;
}

static bool ruda_native_c_block_dominates(
    const RudaMirFunction *function,
    RudaMirBlockId candidate,
    RudaMirBlockId target
) {
    unsigned char *visited = NULL;
    RudaMirBlockId *queue = NULL;
    size_t queue_bytes = 0;
    size_t head = 0, tail = 0, block_count = 0;
    bool dominates = false;
    if (function == NULL || target >= function->blocks.length
        || candidate >= function->blocks.length) return false;
    if (candidate == target) return true;
    block_count = function->blocks.length;
    if (!ruda_size_multiply(
            block_count, sizeof(*queue), &queue_bytes
        )) return false;
    visited = ruda_allocate_array(
        &function->allocator, block_count, sizeof(*visited)
    );
    queue = ruda_allocate(&function->allocator, queue_bytes);
    if (visited == NULL || queue == NULL) goto cleanup;
    memset(visited, 0, block_count * sizeof(*visited));
    if (candidate != 0) {
        visited[0] = 1;
        queue[tail] = 0;
        tail += 1;
    }
    while (head < tail) {
        RudaMirBlockId block_id = queue[head];
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_id
        );
        head += 1;
        if (block == NULL) goto cleanup;
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (!ruda_native_c_block_push_successor(
                    block->terminator.target, candidate, block_count,
                    visited, queue, &tail
                )) goto cleanup;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (!ruda_native_c_block_push_successor(
                    block->terminator.target, candidate, block_count,
                    visited, queue, &tail
                ) || !ruda_native_c_block_push_successor(
                    block->terminator.otherwise, candidate, block_count,
                    visited, queue, &tail
                )) goto cleanup;
        }
    }
    dominates = visited[target] == 0;
cleanup:
    ruda_deallocate(&function->allocator, queue, queue_bytes);
    ruda_deallocate(
        &function->allocator, visited, block_count * sizeof(*visited)
    );
    return dominates;
}

static bool ruda_native_c_array_length_cached(
    const RudaMirFunction *function,
    const RudaMirBlock *block,
    const RudaMirInstruction *instruction
) {
    RudaMirBlockId block_id = RUDA_MIR_NO_BLOCK;
    size_t candidate_index = 0, index = 0;
    if (function == NULL || block == NULL || instruction == NULL
        || !ruda_native_c_array_register_unmodified(
            function, instruction->first
        )) return false;
    for (index = 0; index < function->blocks.length; index += 1) {
        if (ruda_vector_at_const(&function->blocks, index) == block) {
            block_id = (RudaMirBlockId)index;
            break;
        }
    }
    if (block_id == RUDA_MIR_NO_BLOCK) return false;
    for (candidate_index = 0;
        candidate_index < function->blocks.length;
        candidate_index += 1) {
        const RudaMirBlock *candidate = ruda_vector_at_const(
            &function->blocks, candidate_index
        );
        size_t instruction_index = 0;
        if (candidate == NULL || candidate == block
            || !ruda_native_c_block_dominates(
                function, (RudaMirBlockId)candidate_index, block_id
            )) continue;
        for (instruction_index = 0;
            instruction_index < candidate->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *previous = ruda_vector_at_const(
                &candidate->instructions, instruction_index
            );
            if (previous != NULL
                && previous->kind == RUDA_MIR_ARRAY_LENGTH
                && previous->target == instruction->target
                && previous->first == instruction->first
                && previous->array == instruction->array) return true;
        }
    }
    return false;
}

static bool ruda_native_c_emit_scalar_from_bits(
    RudaNativeCBuffer *buffer,
    RudaTypeId type,
    RudaMirRegister target
) {
    if (type == RUDA_TYPE_I64) {
        return ruda_native_c_write(
            buffer,
            " ruda_g_v_%u = ruda_i64_from_twos_complement_fast(ruda_g_bits);",
            target
        );
    }
    if (type == RUDA_TYPE_BOOL) {
        return ruda_native_c_write(
            buffer, " ruda_g_v_%u = ruda_g_bits != 0;", target
        );
    }
    if (type == RUDA_TYPE_U64 || type == RUDA_TYPE_F64) {
        return ruda_native_c_write(
            buffer, " ruda_g_v_%u = ruda_g_bits;", target
        );
    }
    if (type == RUDA_TYPE_F32) {
        return ruda_native_c_write(
            buffer, " ruda_g_v_%u = (uint32_t)ruda_g_bits;", target
        );
    }
    return ruda_native_c_write(
            buffer, " { RudaManagedCell ruda_g_cell = {0}; ruda_g_cell.f64_bits = ruda_g_bits; "
        ) && ruda_native_c_write_from_managed_cell(buffer, type, target)
        && ruda_native_c_write(buffer, "; }");
}

static const RudaMirInstruction *ruda_native_c_array_proof(
    const RudaMirBlock *block,
    size_t instruction_index,
    RudaMirRegister carrier,
    size_t *proof_index
) {
    size_t cursor = instruction_index;
    if (proof_index != NULL) {
        *proof_index = SIZE_MAX;
    }
    while (block != NULL && cursor != 0) {
        const RudaMirInstruction *candidate = NULL;
        cursor -= 1;
        candidate = ruda_vector_at_const(&block->instructions, cursor);
        if (candidate == NULL || candidate->target != carrier) {
            continue;
        }
        if ((candidate->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                || candidate->kind == RUDA_MIR_ARRAY_ASSUME_INDEX)
            && proof_index != NULL) {
            *proof_index = cursor;
            return candidate;
        }
        return NULL;
    }
    return NULL;
}

static bool ruda_native_c_array_require_is_fused(
    const RudaMirBlock *block,
    size_t instruction_index,
    const RudaMirInstruction *instruction
) {
    const RudaMirInstruction *consumer = block != NULL
        ? ruda_vector_at_const(
            &block->instructions, instruction_index + 1
        ) : NULL;
    return instruction != NULL && consumer != NULL
        && (consumer->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS
            || consumer->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS)
        && consumer->first == instruction->target
        && consumer->second == instruction->second;
}

static bool ruda_native_c_array_index_proven(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t function_count,
    const RudaMirInstruction *instruction,
    RudaMirRegister array_register,
    RudaMirRegister index_register
) {
    return ruda_native_c_array_filled_index_proven(
            function, instruction, array_register, index_register
        ) || ruda_native_c_array_relational_index_proven(
            function, functions, function_count, instruction,
            array_register, index_register
        ) || ruda_native_c_array_loaded_index_proven(
            function, functions, function_count, instruction,
            array_register, index_register
        );
}

static bool ruda_native_c_emit_array_bounds_failure(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    return ruda_native_c_write(
            buffer,
            " if (ruda_g_m == RUDA_MANAGED_BOUNDS) { ruda_g_s = ruda_g_begin_bounds(ruda_g_p, ruda_g_v_%u, ruda_g_length, ",
            instruction->second
        ) && ruda_native_c_symbol_data(buffer, function->symbol)
        && ruda_native_c_write(
            buffer,
            ", %zu, %zu); goto ruda_g_cleanup; } if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; }",
            instruction->span.start,
            instruction->span.end
        );
}

static bool ruda_native_c_emit_array_view_bind(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    const RudaMirArray *array
) {
    if (ruda_native_c_array_view_known(function, register_id)) return true;
    return ruda_native_c_write(
        buffer,
        " if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_generated_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }",
        register_id, register_id,
        ruda_native_c_array_descriptor(function, array->type), register_id
    );
}

static bool ruda_native_c_emit_direct_bounds_check(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirRegister index
) {
    return ruda_native_c_write(
            buffer,
            " if (ruda_g_v_%u < 0 || (uint64_t)ruda_g_v_%u >= (uint64_t)ruda_g_length) { ruda_g_s = ruda_g_begin_bounds(ruda_g_p, ruda_g_v_%u, ruda_g_length, ",
            index, index, index
        ) && ruda_native_c_symbol_data(buffer, function->symbol)
        && ruda_native_c_write(
            buffer,
            ", %zu, %zu); goto ruda_g_cleanup; }",
            instruction->span.start,
            instruction->span.end
        );
}

static bool ruda_native_c_emit_array_get(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t function_count,
    const RudaMirInstruction *instruction,
    const RudaMirInstruction *proof,
    bool checked
) {
    const RudaMirArray *array = ruda_vector_at_const(
        &function->arrays, instruction->array
    );
    const RudaMirInstruction *panic_site = checked && proof != NULL
        ? proof : instruction;
    size_t scalar_stride = array != NULL
        ? ruda_native_c_dense_scalar_stride(array->element_type) : 0;
    RudaMirRegister source_origin = RUDA_MIR_NO_REGISTER;
    bool cell = array != NULL
        && (array->element_type == RUDA_TYPE_STRING
            || array->element_type >= RUDA_TYPE_FIRST_DYNAMIC);
    RudaNativeCManagedLayout target_layout;
    bool array_target = cell
        && ruda_native_c_register_array(function, instruction->target) != NULL;
    bool record_target = cell && ruda_native_c_find_managed_layout(
            functions, function_count, array->element_type, &target_layout
        ) && target_layout.kind == RUDA_NATIVE_C_MANAGED_RECORD;
    const RudaMirArray *target_array = array_target
        ? ruda_native_c_register_array(function, instruction->target) : NULL;
    bool prewarmed_children = (array_target || record_target)
        && ruda_native_c_array_register_origin(
            function, instruction->first, &source_origin
        ) && source_origin < function->parameter_count
        && ruda_native_c_array_children_parameter(
            function, functions, function_count, source_origin
        );
    bool snapshot_array = prewarmed_children && target_array != NULL
        && ruda_native_c_dense_scalar_stride(target_array->element_type) != 0
        && ruda_native_c_array_register_unmodified(
            function, instruction->target
        );
    if (scalar_stride != 0) {
        if (!ruda_native_c_write(
                buffer,
                "  { uint64_t ruda_g_bits = 0;"
            ) || !ruda_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            )) {
            return false;
        }
        if (checked
            && (!ruda_native_c_write(
                    buffer,
                    " size_t ruda_g_length = ruda_g_av_%u.object->length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!ruda_native_c_write(
                buffer,
                " ruda_g_bits = ruda_managed_array_generated_get_scalar(ruda_g_av_%u.object, (size_t)ruda_g_v_%u, %zu);",
                instruction->first, instruction->second, scalar_stride
            )) {
            return false;
        }
        return ruda_native_c_emit_scalar_from_bits(
                buffer, array->element_type, instruction->target
            ) && ruda_native_c_write(buffer, " }\n");
    }
    if (cell) {
        if (!ruda_native_c_write(
                buffer, "  { RudaManagedCell ruda_g_cell;"
            ) || !ruda_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            )) {
            return false;
        }
        if (checked
            && (!ruda_native_c_write(
                    buffer,
                    " size_t ruda_g_length = ruda_g_av_%u.object->length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!ruda_native_c_write(
                buffer,
                " memcpy(&ruda_g_cell, ruda_g_av_%u.object->storage + (size_t)ruda_g_v_%u * sizeof(ruda_g_cell), sizeof(ruda_g_cell)); ",
                instruction->first, instruction->second
            ) || !ruda_native_c_write_from_managed_cell(
                buffer, array->element_type, instruction->target
            ) || ((array_target || record_target) && !prewarmed_children
                && !ruda_native_c_write(
                buffer,
                "; if (ruda_g_av_%u.object->generated_children == NULL || ruda_g_av_%u.object->generated_children->generation != ruda_g_av_%u.object->mutation_generation || ruda_g_av_%u.object->generated_children->count != ruda_g_av_%u.object->length) { ruda_g_m = ruda_managed_array_generated_cache_children(&ruda_g_x->heap, ruda_g_v_%u, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }",
                instruction->first, instruction->first, instruction->first,
                instruction->first, instruction->first, instruction->first,
                instruction->first
            )) || (array_target && !ruda_native_c_write(
                    buffer,
                    snapshot_array
                        ? "; ruda_g_av_%u = (RudaManagedArrayGeneratedView){0}; ruda_g_av_%u.object = ((void)ruda_g_av_%u.object, &ruda_g_av_%u.object->generated_children->entries[(size_t)ruda_g_v_%u].array_snapshot)"
                        : "; ruda_g_av_%u = (RudaManagedArrayGeneratedView){0}; ruda_g_av_%u.object = ruda_g_av_%u.object->generated_children != NULL ? (RudaManagedArray *)ruda_g_av_%u.object->generated_children->entries[(size_t)ruda_g_v_%u].object : ruda_managed_array_generated_child_unchecked(&ruda_g_x->heap, ruda_g_cell.reference)",
                    instruction->target, instruction->target,
                    instruction->first, instruction->first,
                    instruction->second
            )) || (record_target && !ruda_native_c_write(
                    buffer,
                    prewarmed_children
                        ? "; ruda_g_rv_%u.reference = ruda_g_cell.reference; ruda_g_rv_%u.object = ((void)ruda_g_av_%u.object, (RudaManagedRecord *)ruda_g_av_%u.object->generated_children->entries[(size_t)ruda_g_v_%u].object)"
                        : "; ruda_g_rv_%u.reference = ruda_g_cell.reference; ruda_g_rv_%u.object = ruda_g_av_%u.object->generated_children != NULL ? (RudaManagedRecord *)ruda_g_av_%u.object->generated_children->entries[(size_t)ruda_g_v_%u].object : ruda_managed_record_generated_child_unchecked(&ruda_g_x->heap, ruda_g_cell.reference)",
                    instruction->target, instruction->target,
                    instruction->first, instruction->first,
                    instruction->second
                ))) {
            return false;
        }
        return ruda_native_c_write(buffer, "; }\n");
    }
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { RudaManagedCell ruda_g_cell; size_t ruda_g_length = 0; RudaManagedArrayView ruda_g_checked;"
        ) || !ruda_native_c_emit_array_view_bind(
            buffer, function, instruction->first, array
        ) || !ruda_native_c_write(
            buffer,
            " ruda_g_checked = ruda_managed_array_generated_expand(&ruda_g_av_%u, ruda_g_v_%u); ruda_g_m = ruda_managed_array_view_get(&ruda_g_checked, ruda_g_v_%u, %s, &ruda_g_cell, &ruda_g_length);",
            instruction->first, instruction->first,
            instruction->second,
            checked ? "RUDA_MANAGED_ARRAY_ACCESS_CHECKED"
                    : "RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS"
        ) || !ruda_native_c_emit_array_bounds_failure(
            buffer, function, panic_site
        ) || !ruda_native_c_write(buffer, " ")
        || !ruda_native_c_write_from_managed_cell(
            buffer, array->element_type, instruction->target
        ) || (ruda_native_c_register_array(function, instruction->target)
            != NULL && !ruda_native_c_write(
                buffer,
                "; ruda_g_av_%u.object = ruda_managed_array_generated_child_unchecked(&ruda_g_x->heap, ruda_g_cell.reference)",
                instruction->target
            ))) {
        return false;
    }
    return ruda_native_c_write(buffer, "; }\n");
}

static bool ruda_native_c_emit_array_set(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaMirInstruction *proof,
    bool checked
) {
    const RudaMirArray *array = ruda_vector_at_const(
        &function->arrays, instruction->array
    );
    const RudaMirInstruction *panic_site = checked && proof != NULL
        ? proof : instruction;
    size_t scalar_stride = array != NULL
        ? ruda_native_c_dense_scalar_stride(array->element_type) : 0;
    if (scalar_stride != 0) {
        if (!ruda_native_c_write(
                buffer, "  { uint64_t ruda_g_bits = (uint64_t)ruda_g_v_%u;",
                instruction->third
            ) || !ruda_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            )) {
            return false;
        }
        if (checked
            && (!ruda_native_c_write(
                    buffer,
                    " size_t ruda_g_length = ruda_g_av_%u.object->length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!(ruda_native_c_batch_scalar_mutation(function)
                ? ruda_native_c_write(
                    buffer,
                    " if (!ruda_g_array_mutated) { if (ruda_g_av_%u.object->mutation_generation == UINT64_MAX) { ruda_g_s = ruda_g_managed_status(RUDA_MANAGED_LIMIT_REACHED); goto ruda_g_cleanup; } ruda_g_av_%u.object->mutation_generation += UINT64_C(1); ruda_g_array_mutated = true; } ruda_managed_array_generated_set_scalar_untracked(ruda_g_av_%u.object, (size_t)ruda_g_v_%u, %zu, ruda_g_bits);",
                    instruction->first, instruction->first,
                    instruction->first, instruction->second, scalar_stride
                ) : ruda_native_c_write(
                    buffer,
                    " if (!ruda_managed_array_generated_set_scalar(ruda_g_av_%u.object, (size_t)ruda_g_v_%u, %zu, ruda_g_bits)) { ruda_g_s = ruda_g_managed_status(RUDA_MANAGED_LIMIT_REACHED); goto ruda_g_cleanup; }",
                    instruction->first, instruction->second, scalar_stride
                ))) {
            return false;
        }
        return ruda_native_c_write(
            buffer,
            " ruda_g_v_%u = ruda_g_v_%u; }\n",
            instruction->target, instruction->third
        );
    }
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { RudaManagedCell ruda_g_cell; size_t ruda_g_length = 0; RudaManagedArrayView ruda_g_checked; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
        ) || !ruda_native_c_write_to_managed_cell(
            buffer, array->element_type, instruction->third
        ) || !ruda_native_c_write(buffer, ";")
        || !ruda_native_c_emit_array_view_bind(
            buffer, function, instruction->first, array
        ) || !ruda_native_c_write(
            buffer,
            " ruda_g_checked = ruda_managed_array_generated_expand(&ruda_g_av_%u, ruda_g_v_%u); ruda_g_m = ruda_managed_array_view_set(&ruda_g_x->heap, &ruda_g_checked, ruda_g_v_%u, %s, ruda_g_cell, &ruda_g_length);",
            instruction->first, instruction->first,
            instruction->second,
            checked ? "RUDA_MANAGED_ARRAY_ACCESS_CHECKED"
                    : "RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS"
        ) || !ruda_native_c_emit_array_bounds_failure(
            buffer, function, panic_site
        )) {
        return false;
    }
    return ruda_native_c_write(
        buffer,
        " ruda_g_v_%u = ruda_g_v_%u; }\n",
        instruction->target,
        instruction->third
    );
}

bool ruda_native_c_emit_range_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t function_count,
    const RudaMirBlock *block,
    size_t instruction_index,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = NULL;
    const RudaMirInstruction *proof = NULL;
    size_t proof_index = SIZE_MAX;
    bool checked = true;
    if (buffer == NULL || function == NULL || block == NULL
        || instruction == NULL) {
        return false;
    }
    array = ruda_vector_at_const(&function->arrays, instruction->array);
    if (array == NULL) {
        return false;
    }
    switch (instruction->kind) {
        case RUDA_MIR_ARRAY_LENGTH:
            if (ruda_native_c_array_length_cached(
                    function, block, instruction
                )) return ruda_native_c_write(buffer, "  (void)0;\n");
            return ruda_native_c_write(
                buffer,
                "  { size_t ruda_g_length;"
            ) && ruda_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            ) && ruda_native_c_write(
                buffer,
                " ruda_g_length = ruda_g_av_%u.object->length; if (ruda_g_length > (size_t)INT64_MAX) { ruda_g_s = 6; goto ruda_g_cleanup; } ruda_g_v_%u = (int64_t)ruda_g_length; }\n",
                instruction->first,
                instruction->target
            );
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
            if (ruda_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                )) {
                return ruda_native_c_write(
                    buffer,
                    "  ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u;\n",
                    instruction->target, instruction->first,
                    instruction->target, instruction->first
                );
            }
            if (ruda_native_c_array_require_is_fused(
                    block, instruction_index, instruction
                )) {
                return ruda_native_c_write(
                    buffer,
                    "  ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u;\n",
                    instruction->target, instruction->first,
                    instruction->target, instruction->first
                );
            }
            if (!ruda_native_c_write(
                    buffer,
                    "  { size_t ruda_g_length;"
                ) || !ruda_native_c_emit_array_view_bind(
                    buffer, function, instruction->first, array
                ) || !ruda_native_c_write(
                    buffer,
                    " ruda_g_length = ruda_g_av_%u.object->length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, instruction, instruction->second
                )) {
                return false;
            }
            return ruda_native_c_write(
                buffer,
                " ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u; }\n",
                instruction->target, instruction->first,
                instruction->target, instruction->first
            );
        case RUDA_MIR_ARRAY_ASSUME_INDEX:
            return ruda_native_c_write(
                buffer,
                "  ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u;\n",
                instruction->target, instruction->first,
                instruction->target, instruction->first
            );
        case RUDA_MIR_ARRAY_GET:
            return ruda_native_c_emit_array_get(
                buffer, function, functions, function_count,
                instruction, NULL,
                !ruda_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                )
            );
        case RUDA_MIR_ARRAY_SET:
            return ruda_native_c_emit_array_set(
                buffer, function, instruction, NULL,
                !ruda_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                )
            );
        case RUDA_MIR_ARRAY_GET_IN_BOUNDS:
        case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
            proof = ruda_native_c_array_proof(
                block, instruction_index, instruction->first, &proof_index
            );
            if (proof == NULL) {
                return false;
            }
            checked = proof->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
                && proof_index + 1 == instruction_index
                && !ruda_native_c_array_index_proven(
                    function, functions, function_count, instruction,
                    instruction->first, instruction->second
                );
            return instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS
                ? ruda_native_c_emit_array_get(
                    buffer, function, functions, function_count,
                    instruction, proof, checked
                )
                : ruda_native_c_emit_array_set(
                    buffer, function, instruction, proof, checked
                );
        default:
            return false;
    }
}
