#include "c_emitter_internal.h"

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
    return ruda_native_c_write(
        buffer,
        " if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_view_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }",
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
    const RudaMirInstruction *instruction,
    const RudaMirInstruction *proof,
    bool checked
) {
    const RudaMirArray *array = ruda_vector_at_const(
        &function->arrays, instruction->array
    );
    const RudaMirInstruction *panic_site = checked && proof != NULL
        ? proof : instruction;
    bool raw64 = array != NULL
        && (array->element_type == RUDA_TYPE_I64
            || array->element_type == RUDA_TYPE_U64
            || array->element_type == RUDA_TYPE_F64);
    bool cell = array != NULL
        && (array->element_type == RUDA_TYPE_STRING
            || array->element_type >= RUDA_TYPE_FIRST_DYNAMIC);
    if (raw64) {
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
                    " size_t ruda_g_length = *ruda_g_av_%u.length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!ruda_native_c_write(
                buffer,
                " memcpy(&ruda_g_bits, *ruda_g_av_%u.storage + (size_t)ruda_g_v_%u * sizeof(ruda_g_bits), sizeof(ruda_g_bits));",
                instruction->first, instruction->second
            )) {
            return false;
        }
        if (array->element_type == RUDA_TYPE_I64) {
            return ruda_native_c_write(
                buffer,
                " ruda_g_v_%u = ruda_i64_from_twos_complement_fast(ruda_g_bits); }\n",
                instruction->target
            );
        }
        return ruda_native_c_write(
            buffer, " ruda_g_v_%u = ruda_g_bits; }\n",
            instruction->target
        );
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
                    " size_t ruda_g_length = *ruda_g_av_%u.length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!ruda_native_c_write(
                buffer,
                " memcpy(&ruda_g_cell, *ruda_g_av_%u.storage + (size_t)ruda_g_v_%u * sizeof(ruda_g_cell), sizeof(ruda_g_cell)); ",
                instruction->first, instruction->second
            ) || !ruda_native_c_write_from_managed_cell(
                buffer, array->element_type, instruction->target
            ) || (ruda_native_c_register_array(function, instruction->target)
                != NULL && !ruda_native_c_write(
                    buffer,
                    "; ruda_g_av_%u = (RudaManagedArrayView){0}",
                    instruction->target
                ))) {
            return false;
        }
        return ruda_native_c_write(buffer, "; }\n");
    }
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { RudaManagedCell ruda_g_cell; size_t ruda_g_length = 0;"
        ) || !ruda_native_c_emit_array_view_bind(
            buffer, function, instruction->first, array
        ) || !ruda_native_c_write(
            buffer,
            " ruda_g_m = ruda_managed_array_view_get(&ruda_g_av_%u, ruda_g_v_%u, %s, &ruda_g_cell, &ruda_g_length);",
            instruction->first,
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
                "; ruda_g_av_%u = (RudaManagedArrayView){0}",
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
    bool raw64 = array != NULL
        && (array->element_type == RUDA_TYPE_I64
            || array->element_type == RUDA_TYPE_U64
            || array->element_type == RUDA_TYPE_F64);
    if (raw64) {
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
                    " size_t ruda_g_length = *ruda_g_av_%u.length;",
                    instruction->first
                ) || !ruda_native_c_emit_direct_bounds_check(
                    buffer, function, panic_site, instruction->second
                ))) {
            return false;
        }
        if (!ruda_native_c_write(
                buffer,
                " if (*ruda_g_av_%u.mutation_generation == UINT64_MAX) { ruda_g_s = ruda_g_managed_status(RUDA_MANAGED_LIMIT_REACHED); goto ruda_g_cleanup; } memcpy(*ruda_g_av_%u.storage + (size_t)ruda_g_v_%u * sizeof(ruda_g_bits), &ruda_g_bits, sizeof(ruda_g_bits)); *ruda_g_av_%u.mutation_generation += UINT64_C(1);",
                instruction->first, instruction->first,
                instruction->second, instruction->first
            )) {
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
            "  { RudaManagedCell ruda_g_cell; size_t ruda_g_length = 0; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
        ) || !ruda_native_c_write_to_managed_cell(
            buffer, array->element_type, instruction->third
        ) || !ruda_native_c_write(buffer, ";")
        || !ruda_native_c_emit_array_view_bind(
            buffer, function, instruction->first, array
        ) || !ruda_native_c_write(
            buffer,
            " ruda_g_m = ruda_managed_array_view_set(&ruda_g_x->heap, &ruda_g_av_%u, ruda_g_v_%u, %s, ruda_g_cell, &ruda_g_length);",
            instruction->first,
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
            return ruda_native_c_write(
                buffer,
                "  { size_t ruda_g_length;"
            ) && ruda_native_c_emit_array_view_bind(
                buffer, function, instruction->first, array
            ) && ruda_native_c_write(
                buffer,
                " ruda_g_length = *ruda_g_av_%u.length; if (ruda_g_length > (size_t)INT64_MAX) { ruda_g_s = 6; goto ruda_g_cleanup; } ruda_g_v_%u = (int64_t)ruda_g_length; }\n",
                instruction->first,
                instruction->target
            );
        case RUDA_MIR_ARRAY_REQUIRE_INDEX:
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
                    " ruda_g_length = *ruda_g_av_%u.length;",
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
                buffer, function, instruction, NULL, true
            );
        case RUDA_MIR_ARRAY_SET:
            return ruda_native_c_emit_array_set(
                buffer, function, instruction, NULL, true
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
                && proof_index + 1 == instruction_index;
            return instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS
                ? ruda_native_c_emit_array_get(
                    buffer, function, instruction, proof, checked
                )
                : ruda_native_c_emit_array_set(
                    buffer, function, instruction, proof, checked
                );
        default:
            return false;
    }
}
