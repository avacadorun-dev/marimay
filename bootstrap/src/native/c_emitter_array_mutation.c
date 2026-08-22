#include "c_emitter_internal.h"

bool ruda_native_c_emit_array_mutation_runtime(RudaNativeCBuffer *buffer) {
    return ruda_native_c_write(
        buffer,
        "static int ruda_g_begin_array_iterator_mutation(ruda_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { static const unsigned char message[] = \"array iterator invalidated by mutation\"; if (p == NULL) return 4; memcpy(p->message_storage, message, sizeof(message) - 1); p->code = \"G4005\"; p->message.data = p->message_storage; p->message.length = sizeof(message) - 1; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return ruda_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n"
    );
}

static const RudaMirArray *ruda_native_c_mutation_array(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    return function != NULL && instruction != NULL
        ? ruda_vector_at_const(&function->arrays, instruction->array)
        : NULL;
}

static bool ruda_native_c_emit_array_type_check(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = ruda_native_c_mutation_array(
        function, instruction
    );
    if (array == NULL) return false;
    if (ruda_native_c_array_register_unmodified(
            function, instruction->first
        ) || ruda_native_c_array_view_known(
            function, instruction->first
        )) return ruda_native_c_write(
        buffer, "ruda_g_m = RUDA_MANAGED_OK; "
    );
    return ruda_native_c_write(
        buffer,
        "const RudaManagedArrayDescriptor *ruda_g_actual = NULL; ruda_g_m = ruda_managed_array_describe(&ruda_g_x->heap, ruda_g_v_%u, &ruda_g_actual); if (ruda_g_m == RUDA_MANAGED_OK && ruda_g_actual != %s) ruda_g_m = RUDA_MANAGED_TYPE_MISMATCH; ",
        instruction->first,
        ruda_native_c_array_descriptor(function, array->type)
    );
}

static bool ruda_native_c_emit_array_pop(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = ruda_native_c_mutation_array(
        function, instruction
    );
    bool raw64 = array != NULL
        && (array->element_type == RUDA_TYPE_I64
            || array->element_type == RUDA_TYPE_U64
            || array->element_type == RUDA_TYPE_F64);
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { RudaManagedCell ruda_g_cell; bool ruda_g_popped = false; "
        ) || !ruda_native_c_write(
            buffer,
            raw64
                ? "if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_generated_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } ruda_g_m = ruda_managed_array_generated_pop_raw64(&ruda_g_av_%u, &ruda_g_cell, &ruda_g_popped); if (ruda_g_m != RUDA_MANAGED_OK || !ruda_g_popped) { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } "
                : "if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_generated_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } { RudaManagedArrayView ruda_g_checked = ruda_managed_array_generated_expand(&ruda_g_av_%u, ruda_g_v_%u); ruda_g_m = ruda_managed_array_view_pop(&ruda_g_checked, &ruda_g_cell, &ruda_g_popped); } if (ruda_g_m != RUDA_MANAGED_OK || !ruda_g_popped) { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ",
            instruction->first, instruction->first,
            ruda_native_c_array_descriptor(function, array->type),
            instruction->first, instruction->first,
            raw64 ? 0 : instruction->first
        ) || (array->element_type == RUDA_TYPE_I64
            ? !ruda_native_c_write(
                buffer,
                "ruda_g_v_%u = ruda_i64_from_twos_complement_fast(ruda_g_cell.f64_bits)",
                instruction->target
            ) : !ruda_native_c_write_from_managed_cell(
                buffer, array->element_type, instruction->target
            )) || (ruda_native_c_register_array(function, instruction->target)
            != NULL && !ruda_native_c_write(
                buffer,
                "; ruda_g_av_%u = (RudaManagedArrayGeneratedView){0}",
                instruction->target
            ))) {
        return false;
    }
    return ruda_native_c_write(buffer, "; }\n");
}

static bool ruda_native_c_emit_array_size_mutation(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const char *operation = instruction->kind == RUDA_MIR_ARRAY_TRUNCATE
        ? "truncate" : instruction->kind == RUDA_MIR_ARRAY_CLEAR
            ? "clear" : "reserve";
    if (!ruda_native_c_write(buffer, "  { ")
        || !ruda_native_c_emit_array_type_check(
            buffer, function, instruction
        ) || !ruda_native_c_write(
            buffer, "if (ruda_g_m == RUDA_MANAGED_OK) ruda_g_m = "
        ) || !ruda_native_c_write(
            buffer,
            instruction->kind == RUDA_MIR_ARRAY_CLEAR
                ? "ruda_managed_array_%s(&ruda_g_x->heap, ruda_g_v_%u); "
                : "ruda_managed_array_%s(&ruda_g_x->heap, ruda_g_v_%u, ruda_g_v_%u); ",
            operation, instruction->first, instruction->second
        ) || !ruda_native_c_write(
            buffer,
            "if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u; }\n",
            instruction->target, instruction->first,
            instruction->target, instruction->first
        )) {
        return false;
    }
    return true;
}

static bool ruda_native_c_emit_array_generation(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = ruda_native_c_mutation_array(
        function, instruction
    );
    return array != NULL && ruda_native_c_write(
        buffer,
        "  { if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_generated_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } ruda_g_v_%u = ruda_g_av_%u.object->mutation_generation; }\n",
        instruction->first, instruction->first,
        ruda_native_c_array_descriptor(function, array->type),
        instruction->first, instruction->target, instruction->first
    );
}

static bool ruda_native_c_array_alias_instruction(
    RudaMirInstructionKind kind
) {
    return kind == RUDA_MIR_COPY
        || kind == RUDA_MIR_ARRAY_REQUIRE_INDEX
        || kind == RUDA_MIR_ARRAY_ASSUME_INDEX
        || kind == RUDA_MIR_ARRAY_REQUIRE_GENERATION
        || kind == RUDA_MIR_ARRAY_PUSH
        || kind == RUDA_MIR_ARRAY_TRUNCATE
        || kind == RUDA_MIR_ARRAY_CLEAR
        || kind == RUDA_MIR_ARRAY_RESERVE;
}

static bool ruda_native_c_copy_origin(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    RudaMirRegister *origin
) {
    size_t step = 0;
    if (function == NULL || origin == NULL) return false;
    for (step = 0; step <= function->register_types.length; step += 1) {
        RudaMirRegister source = RUDA_MIR_NO_REGISTER;
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
                if (instruction == NULL) return false;
                if (instruction->target != register_id
                    || instruction->kind != RUDA_MIR_COPY) continue;
                if (source != RUDA_MIR_NO_REGISTER
                    && source != instruction->first) return false;
                source = instruction->first;
            }
        }
        if (source == RUDA_MIR_NO_REGISTER || source == register_id) {
            *origin = register_id;
            return true;
        }
        register_id = source;
    }
    return false;
}

static bool ruda_native_c_record_array_origin(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    RudaMirRegister *origin
) {
    const RudaMirInstruction *definition = NULL;
    RudaMirRegister receiver_origin = RUDA_MIR_NO_REGISTER;
    RudaMirRegister canonical = register_id;
    size_t block_index = 0;
    if (function == NULL || origin == NULL) return false;
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
            if (instruction == NULL) return false;
            if (instruction->target != register_id
                || instruction->kind != RUDA_MIR_RECORD_GET) continue;
            if (definition != NULL
                && (definition->first != instruction->first
                    || definition->record != instruction->record
                    || definition->field != instruction->field)) return false;
            definition = instruction;
        }
    }
    if (definition == NULL) return false;
    {
        const RudaMirRecord *record = ruda_vector_at_const(
            &function->records, definition->record
        );
        const RudaMirRecordField *field = record != NULL
            && definition->field < record->field_count
            ? ruda_vector_at_const(
                &function->record_fields,
                record->field_start + definition->field
            ) : NULL;
        if (field == NULL || field->is_mutable) return false;
    }
    if (!ruda_native_c_copy_origin(
            function, definition->first, &receiver_origin
        )) return false;
    for (block_index = 0; block_index < function->blocks.length;
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
            RudaMirRegister candidate_receiver = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind != RUDA_MIR_RECORD_GET
                || instruction->record != definition->record
                || instruction->field != definition->field
                || !ruda_native_c_copy_origin(
                    function, instruction->first, &candidate_receiver
                ) || candidate_receiver != receiver_origin) continue;
            if (instruction->target < canonical) {
                canonical = instruction->target;
            }
        }
    }
    *origin = canonical;
    return true;
}

bool ruda_native_c_array_register_origin(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    RudaMirRegister *origin
) {
    size_t step = 0;
    if (function == NULL || origin == NULL) return false;
    for (step = 0; step <= function->register_types.length; step += 1) {
        RudaMirRegister source = RUDA_MIR_NO_REGISTER;
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
                if (instruction == NULL) return false;
                if (instruction->target != register_id
                    || !ruda_native_c_array_alias_instruction(
                        instruction->kind
                    )) continue;
                if (source != RUDA_MIR_NO_REGISTER
                    && source != instruction->first) return false;
                source = instruction->first;
            }
        }
        if (source == RUDA_MIR_NO_REGISTER || source == register_id) {
            if (!ruda_native_c_record_array_origin(
                    function, register_id, origin
                )) *origin = register_id;
            return true;
        }
        register_id = source;
    }
    return false;
}

bool ruda_native_c_array_view_known(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
    size_t block_index = 0;
    size_t array_index = 0;
    bool found = false;
    bool record_elements = false;
    if (function == NULL || !ruda_native_c_array_register_origin(
            function, register_id, &origin
        )) return false;
    if (origin < function->parameter_count) return true;
    for (array_index = 0; array_index < function->arrays.length;
        array_index += 1) {
        const RudaMirArray *array = ruda_vector_at_const(
            &function->arrays, array_index
        );
        if (array != NULL
            && array->element_type >= RUDA_TYPE_FIRST_DYNAMIC
            && ruda_native_c_array(
                function, array->element_type
            ) == NULL) record_elements = true;
    }
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
            if (instruction == NULL) return false;
            if (instruction->target != origin) continue;
            if ((!record_elements
                    || instruction->kind != RUDA_MIR_ARRAY_NEW)
                && instruction->kind != RUDA_MIR_ARRAY_GET
                && instruction->kind != RUDA_MIR_ARRAY_GET_IN_BOUNDS) {
                return false;
            }
            found = true;
        }
    }
    return found;
}

bool ruda_native_c_array_children_parameter(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t function_count,
    RudaMirRegister parameter
) {
    size_t block_index = 0;
    if (function == NULL || parameter >= function->parameter_count
        || ruda_native_c_register_array(function, parameter) == NULL
        || !ruda_native_c_array_register_unmodified(
            function, parameter
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
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
            RudaNativeCManagedLayout layout;
            const RudaTypeId *target_type = NULL;
            bool managed_target = false;
            if (instruction == NULL) return false;
            if (instruction->kind != RUDA_MIR_ARRAY_GET
                && instruction->kind != RUDA_MIR_ARRAY_GET_IN_BOUNDS) {
                continue;
            }
            target_type = ruda_vector_at_const(
                &function->register_types, instruction->target
            );
            managed_target = ruda_native_c_register_array(
                    function, instruction->target
                ) != NULL || (target_type != NULL
                && ruda_native_c_find_managed_layout(
                    functions, function_count,
                    *target_type, &layout
                ) && layout.kind == RUDA_NATIVE_C_MANAGED_RECORD);
            if (managed_target
                && ruda_native_c_array_register_origin(
                    function, instruction->first, &origin
                ) && origin == parameter) return true;
        }
    }
    return false;
}

bool ruda_native_c_array_generation_unobserved(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
    size_t block_index = 0;
    bool local_new = false;
    if (function == NULL || function->calls.length != 0
        || !ruda_native_c_array_register_origin(
            function, register_id, &origin
        ) || origin < function->parameter_count) return false;
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
            RudaMirRegister observed = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->target == origin) {
                if (instruction->kind != RUDA_MIR_ARRAY_NEW) return false;
                local_new = true;
            }
            if (instruction->kind != RUDA_MIR_ARRAY_GENERATION
                && instruction->kind
                    != RUDA_MIR_ARRAY_REQUIRE_GENERATION) continue;
            if (!ruda_native_c_array_register_origin(
                    function, instruction->first, &observed
                ) || observed == origin) return false;
        }
    }
    return local_new;
}

static bool ruda_native_c_array_mutation_instruction(
    RudaMirInstructionKind kind
) {
    return kind == RUDA_MIR_ARRAY_SET
        || kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS
        || kind == RUDA_MIR_ARRAY_PUSH
        || kind == RUDA_MIR_ARRAY_POP
        || kind == RUDA_MIR_ARRAY_TRUNCATE
        || kind == RUDA_MIR_ARRAY_CLEAR
        || kind == RUDA_MIR_ARRAY_RESERVE;
}

bool ruda_native_c_array_register_unmodified(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    RudaMirRegister origin = RUDA_MIR_NO_REGISTER;
    size_t block_index = 0;
    if (function == NULL || function->calls.length != 0
        || !ruda_native_c_array_register_origin(
            function, register_id, &origin
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
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            RudaMirRegister mutation_origin = RUDA_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (!ruda_native_c_array_mutation_instruction(
                    instruction->kind
                )) continue;
            if (!ruda_native_c_array_register_origin(
                    function, instruction->first, &mutation_origin
                ) || mutation_origin == origin) return false;
        }
    }
    return true;
}

static bool ruda_native_c_emit_array_generation_check(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = ruda_native_c_mutation_array(
        function, instruction
    );
    if (array != NULL && ruda_native_c_array_register_unmodified(
            function, instruction->first
        )) {
        return ruda_native_c_write(
            buffer,
            "  ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u;\n",
            instruction->target, instruction->first,
            instruction->target, instruction->first
        );
    }
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_generated_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } if (ruda_g_av_%u.object->mutation_generation != ruda_g_v_%u) { ruda_g_s = ruda_g_begin_array_iterator_mutation(ruda_g_p, ",
            instruction->first, instruction->first,
            ruda_native_c_array_descriptor(function, array->type),
            instruction->first, instruction->first, instruction->second
        ) || !ruda_native_c_symbol_data(buffer, function->symbol)
        || !ruda_native_c_write(
            buffer,
            ", %zu, %zu); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; ruda_g_av_%u = ruda_g_av_%u; }\n",
            instruction->span.start, instruction->span.end,
            instruction->target, instruction->first,
            instruction->target, instruction->first
        )) {
        return false;
    }
    return true;
}

bool ruda_native_c_emit_array_mutation_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    switch (instruction->kind) {
        case RUDA_MIR_ARRAY_POP:
            return ruda_native_c_emit_array_pop(buffer, function, instruction);
        case RUDA_MIR_ARRAY_TRUNCATE:
        case RUDA_MIR_ARRAY_CLEAR:
        case RUDA_MIR_ARRAY_RESERVE:
            return ruda_native_c_emit_array_size_mutation(
                buffer, function, instruction
            );
        case RUDA_MIR_ARRAY_GENERATION:
            return ruda_native_c_emit_array_generation(
                buffer, function, instruction
            );
        case RUDA_MIR_ARRAY_REQUIRE_GENERATION:
            return ruda_native_c_emit_array_generation_check(
                buffer, function, instruction
            );
        default:
            return false;
    }
}
