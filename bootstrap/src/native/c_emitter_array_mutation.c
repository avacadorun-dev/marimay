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
    return array != NULL && ruda_native_c_write(
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
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { RudaManagedCell ruda_g_cell; bool ruda_g_popped = false; "
        ) || !ruda_native_c_write(
            buffer,
            "if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_view_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } ruda_g_m = ruda_managed_array_view_pop(&ruda_g_av_%u, &ruda_g_cell, &ruda_g_popped); if (ruda_g_m != RUDA_MANAGED_OK || !ruda_g_popped) { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ",
            instruction->first, instruction->first,
            ruda_native_c_array_descriptor(function, array->type),
            instruction->first, instruction->first
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
        "  { if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_view_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } ruda_g_v_%u = *ruda_g_av_%u.mutation_generation; }\n",
        instruction->first, instruction->first,
        ruda_native_c_array_descriptor(function, array->type),
        instruction->first, instruction->target, instruction->first
    );
}

static bool ruda_native_c_emit_array_generation_check(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = ruda_native_c_mutation_array(
        function, instruction
    );
    if (array == NULL || !ruda_native_c_write(
            buffer,
            "  { if (ruda_g_av_%u.object == NULL) { ruda_g_m = ruda_managed_array_view_bind(&ruda_g_x->heap, ruda_g_v_%u, %s, &ruda_g_av_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } if (*ruda_g_av_%u.mutation_generation != ruda_g_v_%u) { ruda_g_s = ruda_g_begin_array_iterator_mutation(ruda_g_p, ",
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
