#include "c_emitter_internal.h"

static bool mmay_native_c_emit_defer_record_null_hints(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    size_t parameter
) {
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, parameter) : NULL;
    MarimayNativeCManagedLayout layout;
    const MarimayMirRecord *record = NULL;
    size_t field_index = 0;
    if (function == NULL || parameter >= function->parameter_count
        || type == NULL || *type < MMAY_TYPE_FIRST_DYNAMIC
        || !mmay_native_c_find_managed_layout(
            functions, count, *type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_RECORD) {
        return true;
    }
    record = mmay_vector_at_const(&layout.function->records, layout.index);
    if (record == NULL || !mmay_native_c_write(buffer, ", NULL")) return false;
    for (field_index = 0; field_index < record->field_count;
        field_index += 1) {
        const MarimayMirRecordField *field = mmay_vector_at_const(
            &layout.function->record_fields, record->field_start + field_index
        );
        MarimayNativeCManagedLayout field_layout;
        if (field != NULL && mmay_native_c_find_managed_layout(
                functions, count, field->type, &field_layout
            ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
            && !mmay_native_c_write(buffer, ", NULL")) return false;
    }
    return true;
}

static bool mmay_native_c_emit_defer_direct_call(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirCall *call,
    const char *status,
    const char *panic
) {
    const MarimayMirFunction *callee = call != NULL
        ? mmay_native_c_find(functions, count, call->callee)
        : NULL;
    return callee != NULL
        && mmay_native_c_write(buffer, "%s = ", status)
        && mmay_native_c_symbol(buffer, callee->symbol)
        && mmay_native_c_write(buffer, "(mmay_g_x, %s", panic)
        && mmay_native_c_emit_call_arguments(
            buffer, function, functions, count, call
        )
        && mmay_native_c_write(buffer, ", mmay_g_start, mmay_g_end);");
}

static bool mmay_native_c_emit_defer_interface_call(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirCall *call,
    const char *status,
    const char *panic
) {
    const MarimayMirCallArgument *receiver = call != NULL
        ? mmay_native_c_call_argument(function, call->argument_start)
        : NULL;
    MarimayNativeCManagedLayout layout;
    size_t target = 0;
    if (call == NULL || receiver == NULL
        || !mmay_native_c_find_managed_layout(
            functions, count, receiver->type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_INTERFACE
        || !mmay_native_c_write(
            buffer,
            " { const MarimayManagedInterfaceDescriptor *mmay_g_defer_actual = NULL; MarimayManagedReference mmay_g_defer_payload = mmay_managed_invalid_reference_generated(); const void *mmay_g_defer_table = NULL; uint32_t mmay_g_defer_method = 0; mmay_g_m = mmay_managed_interface_describe(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_defer_actual, &mmay_g_defer_payload, &mmay_g_defer_table); if (mmay_g_m == MMAY_MANAGED_OK && mmay_g_defer_actual != &",
            receiver->register_id
        ) || !mmay_native_c_write_descriptor_name(
            buffer, layout.kind, receiver->type
        ) || !mmay_native_c_write(
            buffer,
            ") mmay_g_m = MMAY_MANAGED_TYPE_MISMATCH; if (mmay_g_m != MMAY_MANAGED_OK || mmay_g_defer_table == NULL) %s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); else { mmay_g_defer_method = ((const uint32_t *)mmay_g_defer_table)[%u]; switch (mmay_g_defer_method) {",
            status, call->interface_slot
        )) {
        return false;
    }
    for (target = 0; target < count; target += 1) {
        size_t argument_index = 1;
        if (!mmay_native_c_method_possible(
                functions, count, receiver->type, call->interface_slot, target
            )) {
            continue;
        }
        if (!mmay_native_c_write(buffer, " case %zu: %s = ", target, status)
            || !mmay_native_c_symbol(buffer, functions[target]->symbol)
            || !mmay_native_c_write(
                buffer, "(mmay_g_x, %s, mmay_g_defer_payload", panic
            ) || !mmay_native_c_emit_defer_record_null_hints(
                buffer, functions[target], functions, count, 0
            )) {
            return false;
        }
        for (argument_index = 1;
            argument_index < call->argument_count; argument_index += 1) {
            const MarimayMirCallArgument *argument = mmay_native_c_call_argument(
                function, call->argument_start + argument_index
            );
            if (argument == NULL || !mmay_native_c_write(
                    buffer, ", mmay_g_v_%u", argument->register_id
                )) {
                return false;
            }
            if (!mmay_native_c_emit_defer_record_null_hints(
                    buffer, functions[target], functions, count,
                    argument_index
                )) {
                return false;
            }
        }
        if (!mmay_native_c_write(
                buffer, ", mmay_g_start, mmay_g_end); break;"
            )) {
            return false;
        }
    }
    return mmay_native_c_write(
        buffer, " default: %s = 7; break; } } }", status
    );
}

static bool mmay_native_c_emit_defer_call_body(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirCall *call,
    const char *status,
    const char *panic
) {
    return call != NULL && call->result_type == MMAY_TYPE_VOID
        && (call->interface_slot == UINT32_MAX
            ? mmay_native_c_emit_defer_direct_call(
                buffer, function, functions, count, call, status, panic
            )
            : mmay_native_c_emit_defer_interface_call(
                buffer, function, functions, count, call, status, panic
            ));
}

bool mmay_native_c_emit_defer_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirCall *call = mmay_native_c_call(
        function, instruction->call
    );
    if (instruction->kind == MMAY_MIR_DEFER_REGISTER) {
        return mmay_native_c_write(
            buffer,
            "  mmay_g_defers[mmay_g_defer_count++] = %u; mmay_g_v_%u = true;\n",
            instruction->call, instruction->target
        );
    }
    return instruction->kind == MMAY_MIR_DEFER_CALL
        && mmay_native_c_write(
            buffer,
            "  if (mmay_g_defer_count == 0 || mmay_g_defers[mmay_g_defer_count - 1] != %u) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_defer_count -= 1; ",
            instruction->call
        )
        && mmay_native_c_emit_defer_call_body(
            buffer, function, functions, count, call, "mmay_g_s", "mmay_g_p"
        )
        && mmay_native_c_write(buffer, "\n")
        && mmay_native_c_emit_propagate_call(
            buffer, function->symbol, instruction->span, false
        );
}

bool mmay_native_c_emit_defer_cleanup(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t call_index = 0;
    if (function->calls.length == 0) {
        return true;
    }
    if (!mmay_native_c_write(
            buffer,
            "  while (mmay_g_defer_count != 0) { uint32_t mmay_g_defer_call = mmay_g_defers[--mmay_g_defer_count]; switch (mmay_g_defer_call) {\n"
        )) {
        return false;
    }
    for (call_index = 0; call_index < function->calls.length;
        call_index += 1) {
        const MarimayMirCall *call = mmay_native_c_call(
            function, (MarimayMirCallId)call_index
        );
        if (call == NULL || call->result_type != MMAY_TYPE_VOID) {
            continue;
        }
        if (!mmay_native_c_write(
            buffer,
            "    case %zu: { int mmay_g_cleanup_s = 0; mmay_g_cleanup_p.frame_count = 0; mmay_g_cleanup_p.suppressed_count = 0; ",
            call_index
            ) || !mmay_native_c_emit_defer_call_body(
                buffer, function, functions, count, call,
                "mmay_g_cleanup_s",
                "mmay_g_s == 4 ? &mmay_g_cleanup_p : mmay_g_p"
            ) || !mmay_native_c_write(
                buffer,
                " if (mmay_g_cleanup_s == 4 && mmay_g_s == 4) { if (!mmay_g_append_suppressed_panic(mmay_g_p, &mmay_g_cleanup_p)) mmay_g_s = 5; } else if (mmay_g_cleanup_s == 4) { mmay_g_s = 4; if (!mmay_g_append_frame(mmay_g_p, "
            ) || !mmay_native_c_symbol_data(buffer, function->symbol)
            || !mmay_native_c_write(
                buffer,
                ", 0, 0)) mmay_g_s = 5; } else if (mmay_g_s == 0 && mmay_g_cleanup_s != 0) mmay_g_s = mmay_g_cleanup_s; break; }\n"
            )) {
            return false;
        }
    }
    return mmay_native_c_write(
        buffer,
        "    default: if (mmay_g_s == 0) mmay_g_s = 7; break; } }\n"
    );
}
