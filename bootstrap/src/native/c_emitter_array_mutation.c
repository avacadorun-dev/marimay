#include "c_emitter_internal.h"

bool mmay_native_c_emit_array_mutation_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(
        buffer,
        "static int mmay_g_begin_array_iterator_mutation(mmay_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { static const unsigned char message[] = \"array iterator invalidated by mutation\"; if (p == NULL) return 4; memcpy(p->message_storage, message, sizeof(message) - 1); p->code = \"G4005\"; p->message.data = p->message_storage; p->message.length = sizeof(message) - 1; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return mmay_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n"
    );
}

static const MarimayMirArray *mmay_native_c_mutation_array(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    return function != NULL && instruction != NULL
        ? mmay_vector_at_const(&function->arrays, instruction->array)
        : NULL;
}

static bool mmay_native_c_emit_array_type_check(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirArray *array = mmay_native_c_mutation_array(
        function, instruction
    );
    if (array == NULL) return false;
    if (mmay_native_c_array_register_unmodified(
            function, instruction->first
        ) || mmay_native_c_array_view_known(
            function, instruction->first
        )) return mmay_native_c_write(
        buffer, "mmay_g_m = MMAY_MANAGED_OK; "
    );
    return mmay_native_c_write(
        buffer,
        "const MarimayManagedArrayDescriptor *mmay_g_actual = NULL; mmay_g_m = mmay_managed_array_describe(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_actual); if (mmay_g_m == MMAY_MANAGED_OK && mmay_g_actual != %s) mmay_g_m = MMAY_MANAGED_TYPE_MISMATCH; ",
        instruction->first,
        mmay_native_c_array_descriptor(function, array->type)
    );
}

static bool mmay_native_c_emit_array_pop(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirArray *array = mmay_native_c_mutation_array(
        function, instruction
    );
    bool raw64 = array != NULL
        && (array->element_type == MMAY_TYPE_I64
            || array->element_type == MMAY_TYPE_U64
            || array->element_type == MMAY_TYPE_F64);
    if (array == NULL || !mmay_native_c_write(
            buffer,
            "  { MarimayManagedCell mmay_g_cell; bool mmay_g_popped = false; "
        ) || !mmay_native_c_write(
            buffer,
            raw64
                ? "if (mmay_g_av_%u.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, %s, &mmay_g_av_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } mmay_g_m = mmay_managed_array_generated_pop_raw64(&mmay_g_av_%u, &mmay_g_cell, &mmay_g_popped); if (mmay_g_m != MMAY_MANAGED_OK || !mmay_g_popped) { mmay_g_s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } "
                : "if (mmay_g_av_%u.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, %s, &mmay_g_av_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } { MarimayManagedArrayView mmay_g_checked = mmay_managed_array_generated_expand(&mmay_g_av_%u, mmay_g_v_%u); mmay_g_m = mmay_managed_array_view_pop(&mmay_g_checked, &mmay_g_cell, &mmay_g_popped); } if (mmay_g_m != MMAY_MANAGED_OK || !mmay_g_popped) { mmay_g_s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } ",
            instruction->first, instruction->first,
            mmay_native_c_array_descriptor(function, array->type),
            instruction->first, instruction->first,
            raw64 ? 0 : instruction->first
        ) || (array->element_type == MMAY_TYPE_I64
            ? !mmay_native_c_write(
                buffer,
                "mmay_g_v_%u = mmay_i64_from_twos_complement_fast(mmay_g_cell.f64_bits)",
                instruction->target
            ) : !mmay_native_c_write_from_managed_cell(
                buffer, array->element_type, instruction->target
            )) || (mmay_native_c_register_array(function, instruction->target)
            != NULL && !mmay_native_c_write(
                buffer,
                "; mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}",
                instruction->target
            ))) {
        return false;
    }
    return mmay_native_c_write(buffer, "; }\n");
}

static bool mmay_native_c_emit_array_size_mutation(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const char *operation = instruction->kind == MMAY_MIR_ARRAY_TRUNCATE
        ? "truncate" : instruction->kind == MMAY_MIR_ARRAY_CLEAR
            ? "clear" : "reserve";
    if (!mmay_native_c_write(buffer, "  { ")
        || !mmay_native_c_emit_array_type_check(
            buffer, function, instruction
        ) || !mmay_native_c_write(
            buffer, "if (mmay_g_m == MMAY_MANAGED_OK) mmay_g_m = "
        ) || !mmay_native_c_write(
            buffer,
            instruction->kind == MMAY_MIR_ARRAY_CLEAR
                ? "mmay_managed_array_%s(&mmay_g_x->heap, mmay_g_v_%u); "
                : "mmay_managed_array_%s(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_v_%u); ",
            operation, instruction->first, instruction->second
        ) || !mmay_native_c_write(
            buffer,
            "if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
            instruction->target, instruction->first,
            instruction->target, instruction->first
        )) {
        return false;
    }
    return true;
}

static bool mmay_native_c_emit_array_generation(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirArray *array = mmay_native_c_mutation_array(
        function, instruction
    );
    return array != NULL && mmay_native_c_write(
        buffer,
        "  { if (mmay_g_av_%u.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, %s, &mmay_g_av_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } mmay_g_v_%u = mmay_g_av_%u.object->mutation_generation; }\n",
        instruction->first, instruction->first,
        mmay_native_c_array_descriptor(function, array->type),
        instruction->first, instruction->target, instruction->first
    );
}

static bool mmay_native_c_array_alias_instruction(
    MarimayMirInstructionKind kind
) {
    return kind == MMAY_MIR_COPY
        || kind == MMAY_MIR_ARRAY_REQUIRE_INDEX
        || kind == MMAY_MIR_ARRAY_ASSUME_INDEX
        || kind == MMAY_MIR_ARRAY_REQUIRE_GENERATION
        || kind == MMAY_MIR_ARRAY_PUSH
        || kind == MMAY_MIR_ARRAY_TRUNCATE
        || kind == MMAY_MIR_ARRAY_CLEAR
        || kind == MMAY_MIR_ARRAY_RESERVE;
}

static bool mmay_native_c_copy_origin(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    MarimayMirRegister *origin
) {
    size_t step = 0;
    if (function == NULL || origin == NULL) return false;
    for (step = 0; step <= function->register_types.length; step += 1) {
        MarimayMirRegister source = MMAY_MIR_NO_REGISTER;
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
                if (instruction == NULL) return false;
                if (instruction->target != register_id
                    || instruction->kind != MMAY_MIR_COPY) continue;
                if (source != MMAY_MIR_NO_REGISTER
                    && source != instruction->first) return false;
                source = instruction->first;
            }
        }
        if (source == MMAY_MIR_NO_REGISTER || source == register_id) {
            *origin = register_id;
            return true;
        }
        register_id = source;
    }
    return false;
}

static bool mmay_native_c_record_array_origin(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    MarimayMirRegister *origin
) {
    const MarimayMirInstruction *definition = NULL;
    MarimayMirRegister receiver_origin = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister canonical = register_id;
    size_t block_index = 0;
    if (function == NULL || origin == NULL) return false;
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
            if (instruction == NULL) return false;
            if (instruction->target != register_id
                || instruction->kind != MMAY_MIR_RECORD_GET) continue;
            if (definition != NULL
                && (definition->first != instruction->first
                    || definition->record != instruction->record
                    || definition->field != instruction->field)) return false;
            definition = instruction;
        }
    }
    if (definition == NULL) return false;
    {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &function->records, definition->record
        );
        const MarimayMirRecordField *field = record != NULL
            && definition->field < record->field_count
            ? mmay_vector_at_const(
                &function->record_fields,
                record->field_start + definition->field
            ) : NULL;
        if (field == NULL || field->is_mutable) return false;
    }
    if (!mmay_native_c_copy_origin(
            function, definition->first, &receiver_origin
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayMirRegister candidate_receiver = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->kind != MMAY_MIR_RECORD_GET
                || instruction->record != definition->record
                || instruction->field != definition->field
                || !mmay_native_c_copy_origin(
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

bool mmay_native_c_array_register_origin(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    MarimayMirRegister *origin
) {
    size_t step = 0;
    if (function == NULL || origin == NULL) return false;
    for (step = 0; step <= function->register_types.length; step += 1) {
        MarimayMirRegister source = MMAY_MIR_NO_REGISTER;
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
                if (instruction == NULL) return false;
                if (instruction->target != register_id
                    || !mmay_native_c_array_alias_instruction(
                        instruction->kind
                    )) continue;
                if (source != MMAY_MIR_NO_REGISTER
                    && source != instruction->first) return false;
                source = instruction->first;
            }
        }
        if (source == MMAY_MIR_NO_REGISTER || source == register_id) {
            if (!mmay_native_c_record_array_origin(
                    function, register_id, origin
                )) *origin = register_id;
            return true;
        }
        register_id = source;
    }
    return false;
}

bool mmay_native_c_array_view_known(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    size_t array_index = 0;
    bool found = false;
    bool record_elements = false;
    if (function == NULL || !mmay_native_c_array_register_origin(
            function, register_id, &origin
        )) return false;
    if (origin < function->parameter_count) return true;
    for (array_index = 0; array_index < function->arrays.length;
        array_index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &function->arrays, array_index
        );
        if (array != NULL
            && array->element_type >= MMAY_TYPE_FIRST_DYNAMIC
            && mmay_native_c_array(
                function, array->element_type
            ) == NULL) record_elements = true;
    }
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
            if (instruction == NULL) return false;
            if (instruction->target != origin) continue;
            if ((!record_elements
                    || instruction->kind != MMAY_MIR_ARRAY_NEW)
                && instruction->kind != MMAY_MIR_ARRAY_GET
                && instruction->kind != MMAY_MIR_ARRAY_GET_IN_BOUNDS) {
                return false;
            }
            found = true;
        }
    }
    return found;
}

bool mmay_native_c_array_children_parameter(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimayMirRegister parameter
) {
    size_t block_index = 0;
    if (function == NULL || parameter >= function->parameter_count
        || mmay_native_c_register_array(function, parameter) == NULL
        || !mmay_native_c_array_register_unmodified(
            function, parameter
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
            MarimayNativeCManagedLayout layout;
            const MarimayTypeId *target_type = NULL;
            bool managed_target = false;
            if (instruction == NULL) return false;
            if (instruction->kind != MMAY_MIR_ARRAY_GET
                && instruction->kind != MMAY_MIR_ARRAY_GET_IN_BOUNDS) {
                continue;
            }
            target_type = mmay_vector_at_const(
                &function->register_types, instruction->target
            );
            managed_target = mmay_native_c_register_array(
                    function, instruction->target
                ) != NULL || (target_type != NULL
                && mmay_native_c_find_managed_layout(
                    functions, function_count,
                    *target_type, &layout
                ) && layout.kind == MMAY_NATIVE_C_MANAGED_RECORD);
            if (managed_target
                && mmay_native_c_array_register_origin(
                    function, instruction->first, &origin
                ) && origin == parameter) return true;
        }
    }
    return false;
}

bool mmay_native_c_array_generation_unobserved(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    bool local_new = false;
    if (function == NULL || function->calls.length != 0
        || !mmay_native_c_array_register_origin(
            function, register_id, &origin
        ) || origin < function->parameter_count) return false;
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
            MarimayMirRegister observed = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (instruction->target == origin) {
                if (instruction->kind != MMAY_MIR_ARRAY_NEW) return false;
                local_new = true;
            }
            if (instruction->kind != MMAY_MIR_ARRAY_GENERATION
                && instruction->kind
                    != MMAY_MIR_ARRAY_REQUIRE_GENERATION) continue;
            if (!mmay_native_c_array_register_origin(
                    function, instruction->first, &observed
                ) || observed == origin) return false;
        }
    }
    return local_new;
}

static bool mmay_native_c_array_mutation_instruction(
    MarimayMirInstructionKind kind
) {
    return kind == MMAY_MIR_ARRAY_SET
        || kind == MMAY_MIR_ARRAY_SET_IN_BOUNDS
        || kind == MMAY_MIR_ARRAY_PUSH
        || kind == MMAY_MIR_ARRAY_POP
        || kind == MMAY_MIR_ARRAY_TRUNCATE
        || kind == MMAY_MIR_ARRAY_CLEAR
        || kind == MMAY_MIR_ARRAY_RESERVE;
}

bool mmay_native_c_array_register_unmodified(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    MarimayMirRegister origin = MMAY_MIR_NO_REGISTER;
    size_t block_index = 0;
    if (function == NULL || function->calls.length != 0
        || !mmay_native_c_array_register_origin(
            function, register_id, &origin
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
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            MarimayMirRegister mutation_origin = MMAY_MIR_NO_REGISTER;
            if (instruction == NULL) return false;
            if (!mmay_native_c_array_mutation_instruction(
                    instruction->kind
                )) continue;
            if (!mmay_native_c_array_register_origin(
                    function, instruction->first, &mutation_origin
                ) || mutation_origin == origin) return false;
        }
    }
    return true;
}

static bool mmay_native_c_emit_array_generation_check(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirArray *array = mmay_native_c_mutation_array(
        function, instruction
    );
    if (array == NULL || !mmay_native_c_write(
            buffer,
            "  { if (mmay_g_av_%u.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, %s, &mmay_g_av_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } if (mmay_g_av_%u.object->mutation_generation != mmay_g_v_%u) { mmay_g_s = mmay_g_begin_array_iterator_mutation(mmay_g_p, ",
            instruction->first, instruction->first,
            mmay_native_c_array_descriptor(function, array->type),
            instruction->first, instruction->first, instruction->second
        ) || !mmay_native_c_symbol_data(buffer, function->symbol)
        || !mmay_native_c_write(
            buffer,
            ", %zu, %zu); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
            instruction->span.start, instruction->span.end,
            instruction->target, instruction->first,
            instruction->target, instruction->first
        )) {
        return false;
    }
    return true;
}

bool mmay_native_c_emit_array_mutation_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    switch (instruction->kind) {
        case MMAY_MIR_ARRAY_POP:
            return mmay_native_c_emit_array_pop(buffer, function, instruction);
        case MMAY_MIR_ARRAY_TRUNCATE:
        case MMAY_MIR_ARRAY_CLEAR:
        case MMAY_MIR_ARRAY_RESERVE:
            return mmay_native_c_emit_array_size_mutation(
                buffer, function, instruction
            );
        case MMAY_MIR_ARRAY_GENERATION:
            return mmay_native_c_emit_array_generation(
                buffer, function, instruction
            );
        case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
            return mmay_native_c_emit_array_generation_check(
                buffer, function, instruction
            );
        default:
            return false;
    }
}
