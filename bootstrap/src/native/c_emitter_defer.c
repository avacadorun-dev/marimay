#include "c_emitter_internal.h"

static bool ruda_native_c_emit_defer_record_null_hints(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    size_t parameter
) {
    const RudaTypeId *type = function != NULL
        ? ruda_vector_at_const(&function->register_types, parameter) : NULL;
    RudaNativeCManagedLayout layout;
    const RudaMirRecord *record = NULL;
    size_t field_index = 0;
    if (function == NULL || parameter >= function->parameter_count
        || type == NULL || *type < RUDA_TYPE_FIRST_DYNAMIC
        || !ruda_native_c_find_managed_layout(
            functions, count, *type, &layout
        ) || layout.kind != RUDA_NATIVE_C_MANAGED_RECORD) {
        return true;
    }
    record = ruda_vector_at_const(&layout.function->records, layout.index);
    if (record == NULL || !ruda_native_c_write(buffer, ", NULL")) return false;
    for (field_index = 0; field_index < record->field_count;
        field_index += 1) {
        const RudaMirRecordField *field = ruda_vector_at_const(
            &layout.function->record_fields, record->field_start + field_index
        );
        RudaNativeCManagedLayout field_layout;
        if (field != NULL && ruda_native_c_find_managed_layout(
                functions, count, field->type, &field_layout
            ) && field_layout.kind == RUDA_NATIVE_C_MANAGED_ARRAY
            && !ruda_native_c_write(buffer, ", NULL")) return false;
    }
    return true;
}

static bool ruda_native_c_emit_defer_direct_call(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaMirCall *call,
    const char *status,
    const char *panic
) {
    const RudaMirFunction *callee = call != NULL
        ? ruda_native_c_find(functions, count, call->callee)
        : NULL;
    return callee != NULL
        && ruda_native_c_write(buffer, "%s = ", status)
        && ruda_native_c_symbol(buffer, callee->symbol)
        && ruda_native_c_write(buffer, "(ruda_g_x, %s", panic)
        && ruda_native_c_emit_call_arguments(
            buffer, function, functions, count, call
        )
        && ruda_native_c_write(buffer, ", ruda_g_start, ruda_g_end);");
}

static bool ruda_native_c_emit_defer_interface_call(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaMirCall *call,
    const char *status,
    const char *panic
) {
    const RudaMirCallArgument *receiver = call != NULL
        ? ruda_native_c_call_argument(function, call->argument_start)
        : NULL;
    RudaNativeCManagedLayout layout;
    size_t target = 0;
    if (call == NULL || receiver == NULL
        || !ruda_native_c_find_managed_layout(
            functions, count, receiver->type, &layout
        ) || layout.kind != RUDA_NATIVE_C_MANAGED_INTERFACE
        || !ruda_native_c_write(
            buffer,
            " { const RudaManagedInterfaceDescriptor *ruda_g_defer_actual = NULL; RudaManagedReference ruda_g_defer_payload = ruda_managed_invalid_reference_generated(); const void *ruda_g_defer_table = NULL; uint32_t ruda_g_defer_method = 0; ruda_g_m = ruda_managed_interface_describe(&ruda_g_x->heap, ruda_g_v_%u, &ruda_g_defer_actual, &ruda_g_defer_payload, &ruda_g_defer_table); if (ruda_g_m == RUDA_MANAGED_OK && ruda_g_defer_actual != &",
            receiver->register_id
        ) || !ruda_native_c_write_descriptor_name(
            buffer, layout.kind, receiver->type
        ) || !ruda_native_c_write(
            buffer,
            ") ruda_g_m = RUDA_MANAGED_TYPE_MISMATCH; if (ruda_g_m != RUDA_MANAGED_OK || ruda_g_defer_table == NULL) %s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); else { ruda_g_defer_method = ((const uint32_t *)ruda_g_defer_table)[%u]; switch (ruda_g_defer_method) {",
            status, call->interface_slot
        )) {
        return false;
    }
    for (target = 0; target < count; target += 1) {
        size_t argument_index = 1;
        if (!ruda_native_c_method_possible(
                functions, count, receiver->type, call->interface_slot, target
            )) {
            continue;
        }
        if (!ruda_native_c_write(buffer, " case %zu: %s = ", target, status)
            || !ruda_native_c_symbol(buffer, functions[target]->symbol)
            || !ruda_native_c_write(
                buffer, "(ruda_g_x, %s, ruda_g_defer_payload", panic
            ) || !ruda_native_c_emit_defer_record_null_hints(
                buffer, functions[target], functions, count, 0
            )) {
            return false;
        }
        for (argument_index = 1;
            argument_index < call->argument_count; argument_index += 1) {
            const RudaMirCallArgument *argument = ruda_native_c_call_argument(
                function, call->argument_start + argument_index
            );
            if (argument == NULL || !ruda_native_c_write(
                    buffer, ", ruda_g_v_%u", argument->register_id
                )) {
                return false;
            }
            if (!ruda_native_c_emit_defer_record_null_hints(
                    buffer, functions[target], functions, count,
                    argument_index
                )) {
                return false;
            }
        }
        if (!ruda_native_c_write(
                buffer, ", ruda_g_start, ruda_g_end); break;"
            )) {
            return false;
        }
    }
    return ruda_native_c_write(
        buffer, " default: %s = 7; break; } } }", status
    );
}

static bool ruda_native_c_emit_defer_call_body(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaMirCall *call,
    const char *status,
    const char *panic
) {
    return call != NULL && call->result_type == RUDA_TYPE_VOID
        && (call->interface_slot == UINT32_MAX
            ? ruda_native_c_emit_defer_direct_call(
                buffer, function, functions, count, call, status, panic
            )
            : ruda_native_c_emit_defer_interface_call(
                buffer, function, functions, count, call, status, panic
            ));
}

bool ruda_native_c_emit_defer_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaMirInstruction *instruction
) {
    const RudaMirCall *call = ruda_native_c_call(
        function, instruction->call
    );
    if (instruction->kind == RUDA_MIR_DEFER_REGISTER) {
        return ruda_native_c_write(
            buffer,
            "  ruda_g_defers[ruda_g_defer_count++] = %u; ruda_g_v_%u = true;\n",
            instruction->call, instruction->target
        );
    }
    return instruction->kind == RUDA_MIR_DEFER_CALL
        && ruda_native_c_write(
            buffer,
            "  if (ruda_g_defer_count == 0 || ruda_g_defers[ruda_g_defer_count - 1] != %u) { ruda_g_s = 7; goto ruda_g_cleanup; } ruda_g_defer_count -= 1; ",
            instruction->call
        )
        && ruda_native_c_emit_defer_call_body(
            buffer, function, functions, count, call, "ruda_g_s", "ruda_g_p"
        )
        && ruda_native_c_write(buffer, "\n")
        && ruda_native_c_emit_propagate_call(
            buffer, function->symbol, instruction->span, false
        );
}

bool ruda_native_c_emit_defer_cleanup(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t call_index = 0;
    if (function->calls.length == 0) {
        return true;
    }
    if (!ruda_native_c_write(
            buffer,
            "  while (ruda_g_defer_count != 0) { uint32_t ruda_g_defer_call = ruda_g_defers[--ruda_g_defer_count]; switch (ruda_g_defer_call) {\n"
        )) {
        return false;
    }
    for (call_index = 0; call_index < function->calls.length;
        call_index += 1) {
        const RudaMirCall *call = ruda_native_c_call(
            function, (RudaMirCallId)call_index
        );
        if (call == NULL || call->result_type != RUDA_TYPE_VOID) {
            continue;
        }
        if (!ruda_native_c_write(
            buffer,
            "    case %zu: { int ruda_g_cleanup_s = 0; ruda_g_cleanup_p.frame_count = 0; ruda_g_cleanup_p.suppressed_count = 0; ",
            call_index
            ) || !ruda_native_c_emit_defer_call_body(
                buffer, function, functions, count, call,
                "ruda_g_cleanup_s",
                "ruda_g_s == 4 ? &ruda_g_cleanup_p : ruda_g_p"
            ) || !ruda_native_c_write(
                buffer,
                " if (ruda_g_cleanup_s == 4 && ruda_g_s == 4) { if (!ruda_g_append_suppressed_panic(ruda_g_p, &ruda_g_cleanup_p)) ruda_g_s = 5; } else if (ruda_g_cleanup_s == 4) { ruda_g_s = 4; if (!ruda_g_append_frame(ruda_g_p, "
            ) || !ruda_native_c_symbol_data(buffer, function->symbol)
            || !ruda_native_c_write(
                buffer,
                ", 0, 0)) ruda_g_s = 5; } else if (ruda_g_s == 0 && ruda_g_cleanup_s != 0) ruda_g_s = ruda_g_cleanup_s; break; }\n"
            )) {
            return false;
        }
    }
    return ruda_native_c_write(
        buffer,
        "    default: if (ruda_g_s == 0) ruda_g_s = 7; break; } }\n"
    );
}
