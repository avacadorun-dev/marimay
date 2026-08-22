#include "c_emitter_internal.h"

static const MarimayMirFunction *mmay_native_c_executable_message(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimaySymbolId symbol
) {
    return mmay_native_c_find(functions, count, symbol);
}

MarimayNativeCEmitStatus mmay_native_c_validate_executable_entry(
    const MarimayMirFunction *entry,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
) {
    MarimayNativeCManagedLayout layout;
    const MarimayMirEnum *enumeration = NULL;
    const MarimayMirEnumVariant *ok = NULL;
    const MarimayMirEnumVariant *error = NULL;
    const MarimayMirEnumPayload *payload = NULL;
    const MarimayMirFunction *message = NULL;
    const MarimayTypeId *parameter_type = NULL;
    if (entry == NULL || functions == NULL || options == NULL) {
        return MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT;
    }
    if (!options->executable_returns_result) {
        return entry->result_type == MMAY_TYPE_VOID
            ? MMAY_NATIVE_C_EMIT_OK : MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    if (!mmay_native_c_find_managed_layout(
            functions, count, entry->result_type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_ENUM) {
        return MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    enumeration = mmay_vector_at_const(&layout.function->enums, layout.index);
    ok = enumeration == NULL ? NULL : mmay_vector_at_const(
        &layout.function->enum_variants, enumeration->variant_start
    );
    error = enumeration == NULL ? NULL : mmay_vector_at_const(
        &layout.function->enum_variants, enumeration->variant_start + 1
    );
    payload = error == NULL ? NULL : mmay_vector_at_const(
        &layout.function->enum_payloads, error->payload_start
    );
    message = mmay_native_c_executable_message(
        functions, count, options->error_message_symbol
    );
    parameter_type = message == NULL ? NULL
        : mmay_vector_at_const(&message->register_types, 0);
    if (enumeration == NULL || enumeration->variant_count != 2
        || ok == NULL || ok->payload_count != 0
        || error == NULL || error->payload_count != 1 || payload == NULL
        || payload->type < MMAY_TYPE_FIRST_DYNAMIC
        || message == NULL || message->parameter_count != 1
        || parameter_type == NULL || *parameter_type != payload->type
        || message->result_type != MMAY_TYPE_STRING) {
        return MMAY_NATIVE_C_EMIT_INVALID_MIR;
    }
    return MMAY_NATIVE_C_EMIT_OK;
}

bool mmay_native_c_emit_public_helpers(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(buffer,
        "static int mmay_g_public_message(const char *kind, const char *code, const unsigned char *data, size_t length, int exit_code) { if (fputs(kind, stderr) == EOF || fputc('[', stderr) == EOF || fputs(code, stderr) == EOF || fputs(\"]: \", stderr) == EOF || (length != 0 && fwrite(data, 1, length, stderr) != length) || fputc('\\n', stderr) == EOF || fflush(stderr) != 0) return 70; return exit_code; }\n"
        "static int mmay_g_public_budget(const MarimayManagedHeap *heap) { MarimayManagedResourceFailure failure; if (heap == NULL || !mmay_managed_heap_last_resource_failure(heap, &failure) || failure.classification != MMAY_MANAGED_RESOURCE_FAILURE_BUDGET) return 0; if (fprintf(stderr, \"error[I0002]: managed heap budget exceeded: live=%%zu bytes, requested=%%zu bytes, limit=%%zu bytes\\n\", failure.live_bytes, failure.requested_bytes, failure.limit_bytes) < 0 || fflush(stderr) != 0) return 70; return 70; }\n"
        "static int mmay_g_public_status(int status, const mmay_g_panic *panic, const MarimayManagedHeap *heap) { static const unsigned char overflow[] = \"integer overflow\"; static const unsigned char division[] = \"integer division by zero\"; static const unsigned char shift[] = \"integer shift count is outside width\"; static const unsigned char allocation[] = \"runtime host allocation failed\"; static const unsigned char limit[] = \"runtime resource limit reached\"; static const unsigned char integrity[] = \"native runtime integrity failure\"; int budget = 0; if (status == 4 && panic != NULL) return mmay_g_public_message(\"panic\", panic->code, panic->message.data, panic->message.length, 1); if (status == 1) return mmay_g_public_message(\"panic\", \"G4001\", overflow, sizeof(overflow) - 1, 1); if (status == 2) return mmay_g_public_message(\"panic\", \"G4001\", division, sizeof(division) - 1, 1); if (status == 3) return mmay_g_public_message(\"panic\", \"G4001\", shift, sizeof(shift) - 1, 1); if (status == 5) return mmay_g_public_message(\"error\", \"I0002\", allocation, sizeof(allocation) - 1, 70); if (status == 6) { budget = mmay_g_public_budget(heap); if (budget != 0) return budget; return mmay_g_public_message(\"error\", \"I0002\", limit, sizeof(limit) - 1, 70); } return mmay_g_public_message(\"error\", \"I0002\", integrity, sizeof(integrity) - 1, 70); }\n\n"
    );
}

bool mmay_native_c_emit_executable_entry(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *entry,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayNativeCEmitOptions *options
) {
    MarimayNativeCManagedLayout layout;
    const MarimayMirFunction *message = NULL;
    if (buffer == NULL || entry == NULL || functions == NULL || options == NULL) {
        return false;
    }
    if (!options->executable_returns_result) {
        return mmay_native_c_write(buffer, "mmay_g_s = ")
            && mmay_native_c_symbol(buffer, entry->symbol)
            && mmay_native_c_write(buffer,
                "(&mmay_g_x, &mmay_g_p, &mmay_g_start, &mmay_g_end); { int mmay_g_exit = mmay_g_s == 0 ? 0 : mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); mmay_g_execution_dispose(&mmay_g_x); return mmay_g_exit; } }\n"
            );
    }
    if (!mmay_native_c_find_managed_layout(
            functions, count, entry->result_type, &layout
        )) return false;
    message = mmay_native_c_executable_message(
        functions, count, options->error_message_symbol
    );
    return mmay_native_c_write(
            buffer,
            "MarimayManagedReference mmay_g_result = mmay_managed_invalid_reference_generated(), mmay_g_error = mmay_managed_invalid_reference_generated(); MarimayManagedReference *mmay_g_roots[2] = {&mmay_g_result, &mmay_g_error}; MarimayManagedRootFrame mmay_g_root_frame = {0}; const MarimayManagedEnumDescriptor *mmay_g_descriptor = NULL; size_t mmay_g_variant = 0; MarimayManagedCell mmay_g_cell; mmay_g_string mmay_g_error_message = {0}; int mmay_g_exit = 70; mmay_g_s = "
        ) && mmay_native_c_symbol(buffer, entry->symbol)
        && mmay_native_c_write(buffer,
            "(&mmay_g_x, &mmay_g_p, &mmay_g_result, &mmay_g_start, &mmay_g_end); if (mmay_g_s != 0) { mmay_g_exit = mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup; } if (mmay_managed_root_push(&mmay_g_x.heap, &mmay_g_root_frame, mmay_g_roots, 2) != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup; } if (mmay_managed_enum_describe(&mmay_g_x.heap, mmay_g_result, &mmay_g_descriptor, &mmay_g_variant) != MMAY_MANAGED_OK || mmay_g_descriptor != &"
        ) && mmay_native_c_write_descriptor_name(
            buffer, layout.kind, entry->result_type
        ) && mmay_native_c_write(buffer,
            ") { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_root_cleanup; } if (mmay_g_variant == 0) { mmay_g_exit = 0; goto mmay_g_public_root_cleanup; } if (mmay_g_variant != 1 || mmay_managed_enum_get_payload(&mmay_g_x.heap, mmay_g_result, 1, 0, &mmay_g_cell) != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_root_cleanup; } mmay_g_error = mmay_g_cell.reference; mmay_g_s = "
        ) && mmay_native_c_symbol(buffer, message->symbol)
        && mmay_native_c_write(buffer,
            "(&mmay_g_x, &mmay_g_p, mmay_g_error, &mmay_g_error_message, &mmay_g_start, &mmay_g_end); if (mmay_g_s != 0) mmay_g_exit = mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); else mmay_g_exit = mmay_g_public_message(\"error\", \"G4003\", mmay_g_error_message.data, mmay_g_error_message.length, 1); mmay_g_public_root_cleanup: if (mmay_managed_root_pop(&mmay_g_x.heap, &mmay_g_root_frame) != MMAY_MANAGED_OK) mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); mmay_g_public_cleanup: mmay_g_execution_dispose(&mmay_g_x); return mmay_g_exit; }\n"
        );
}
