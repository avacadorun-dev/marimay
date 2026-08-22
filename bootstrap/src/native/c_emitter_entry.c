#include "c_emitter_internal.h"

static const RudaMirFunction *ruda_native_c_executable_message(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaSymbolId symbol
) {
    return ruda_native_c_find(functions, count, symbol);
}

RudaNativeCEmitStatus ruda_native_c_validate_executable_entry(
    const RudaMirFunction *entry,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaNativeCEmitOptions *options
) {
    RudaNativeCManagedLayout layout;
    const RudaMirEnum *enumeration = NULL;
    const RudaMirEnumVariant *ok = NULL;
    const RudaMirEnumVariant *error = NULL;
    const RudaMirEnumPayload *payload = NULL;
    const RudaMirFunction *message = NULL;
    const RudaTypeId *parameter_type = NULL;
    if (entry == NULL || functions == NULL || options == NULL) {
        return RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT;
    }
    if (!options->executable_returns_result) {
        return entry->result_type == RUDA_TYPE_VOID
            ? RUDA_NATIVE_C_EMIT_OK : RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    if (!ruda_native_c_find_managed_layout(
            functions, count, entry->result_type, &layout
        ) || layout.kind != RUDA_NATIVE_C_MANAGED_ENUM) {
        return RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    enumeration = ruda_vector_at_const(&layout.function->enums, layout.index);
    ok = enumeration == NULL ? NULL : ruda_vector_at_const(
        &layout.function->enum_variants, enumeration->variant_start
    );
    error = enumeration == NULL ? NULL : ruda_vector_at_const(
        &layout.function->enum_variants, enumeration->variant_start + 1
    );
    payload = error == NULL ? NULL : ruda_vector_at_const(
        &layout.function->enum_payloads, error->payload_start
    );
    message = ruda_native_c_executable_message(
        functions, count, options->error_message_symbol
    );
    parameter_type = message == NULL ? NULL
        : ruda_vector_at_const(&message->register_types, 0);
    if (enumeration == NULL || enumeration->variant_count != 2
        || ok == NULL || ok->payload_count != 0
        || error == NULL || error->payload_count != 1 || payload == NULL
        || payload->type < RUDA_TYPE_FIRST_DYNAMIC
        || message == NULL || message->parameter_count != 1
        || parameter_type == NULL || *parameter_type != payload->type
        || message->result_type != RUDA_TYPE_STRING) {
        return RUDA_NATIVE_C_EMIT_INVALID_MIR;
    }
    return RUDA_NATIVE_C_EMIT_OK;
}

bool ruda_native_c_emit_public_helpers(RudaNativeCBuffer *buffer) {
    return ruda_native_c_write(buffer,
        "static int ruda_g_public_message(const char *kind, const char *code, const unsigned char *data, size_t length, int exit_code) { if (fputs(kind, stderr) == EOF || fputc('[', stderr) == EOF || fputs(code, stderr) == EOF || fputs(\"]: \", stderr) == EOF || (length != 0 && fwrite(data, 1, length, stderr) != length) || fputc('\\n', stderr) == EOF || fflush(stderr) != 0) return 70; return exit_code; }\n"
        "static int ruda_g_public_budget(const RudaManagedHeap *heap) { RudaManagedResourceFailure failure; if (heap == NULL || !ruda_managed_heap_last_resource_failure(heap, &failure) || failure.classification != RUDA_MANAGED_RESOURCE_FAILURE_BUDGET) return 0; if (fprintf(stderr, \"error[I0002]: managed heap budget exceeded: live=%%zu bytes, requested=%%zu bytes, limit=%%zu bytes\\n\", failure.live_bytes, failure.requested_bytes, failure.limit_bytes) < 0 || fflush(stderr) != 0) return 70; return 70; }\n"
        "static int ruda_g_public_status(int status, const ruda_g_panic *panic, const RudaManagedHeap *heap) { static const unsigned char overflow[] = \"integer overflow\"; static const unsigned char division[] = \"integer division by zero\"; static const unsigned char shift[] = \"integer shift count is outside width\"; static const unsigned char allocation[] = \"runtime host allocation failed\"; static const unsigned char limit[] = \"runtime resource limit reached\"; static const unsigned char integrity[] = \"native runtime integrity failure\"; int budget = 0; if (status == 4 && panic != NULL) return ruda_g_public_message(\"panic\", panic->code, panic->message.data, panic->message.length, 1); if (status == 1) return ruda_g_public_message(\"panic\", \"G4001\", overflow, sizeof(overflow) - 1, 1); if (status == 2) return ruda_g_public_message(\"panic\", \"G4001\", division, sizeof(division) - 1, 1); if (status == 3) return ruda_g_public_message(\"panic\", \"G4001\", shift, sizeof(shift) - 1, 1); if (status == 5) return ruda_g_public_message(\"error\", \"I0002\", allocation, sizeof(allocation) - 1, 70); if (status == 6) { budget = ruda_g_public_budget(heap); if (budget != 0) return budget; return ruda_g_public_message(\"error\", \"I0002\", limit, sizeof(limit) - 1, 70); } return ruda_g_public_message(\"error\", \"I0002\", integrity, sizeof(integrity) - 1, 70); }\n\n"
    );
}

bool ruda_native_c_emit_executable_entry(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *entry,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaNativeCEmitOptions *options
) {
    RudaNativeCManagedLayout layout;
    const RudaMirFunction *message = NULL;
    if (buffer == NULL || entry == NULL || functions == NULL || options == NULL) {
        return false;
    }
    if (!options->executable_returns_result) {
        return ruda_native_c_write(buffer, "ruda_g_s = ")
            && ruda_native_c_symbol(buffer, entry->symbol)
            && ruda_native_c_write(buffer,
                "(&ruda_g_x, &ruda_g_p, &ruda_g_start, &ruda_g_end); { int ruda_g_exit = ruda_g_s == 0 ? 0 : ruda_g_public_status(ruda_g_s, &ruda_g_p, &ruda_g_x.heap); ruda_g_execution_dispose(&ruda_g_x); return ruda_g_exit; } }\n"
            );
    }
    if (!ruda_native_c_find_managed_layout(
            functions, count, entry->result_type, &layout
        )) return false;
    message = ruda_native_c_executable_message(
        functions, count, options->error_message_symbol
    );
    return ruda_native_c_write(
            buffer,
            "RudaManagedReference ruda_g_result = ruda_managed_invalid_reference_generated(), ruda_g_error = ruda_managed_invalid_reference_generated(); RudaManagedReference *ruda_g_roots[2] = {&ruda_g_result, &ruda_g_error}; RudaManagedRootFrame ruda_g_root_frame = {0}; const RudaManagedEnumDescriptor *ruda_g_descriptor = NULL; size_t ruda_g_variant = 0; RudaManagedCell ruda_g_cell; ruda_g_string ruda_g_error_message = {0}; int ruda_g_exit = 70; ruda_g_s = "
        ) && ruda_native_c_symbol(buffer, entry->symbol)
        && ruda_native_c_write(buffer,
            "(&ruda_g_x, &ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s != 0) { ruda_g_exit = ruda_g_public_status(ruda_g_s, &ruda_g_p, &ruda_g_x.heap); goto ruda_g_public_cleanup; } if (ruda_managed_root_push(&ruda_g_x.heap, &ruda_g_root_frame, ruda_g_roots, 2) != RUDA_MANAGED_OK) { ruda_g_exit = ruda_g_public_status(7, &ruda_g_p, &ruda_g_x.heap); goto ruda_g_public_cleanup; } if (ruda_managed_enum_describe(&ruda_g_x.heap, ruda_g_result, &ruda_g_descriptor, &ruda_g_variant) != RUDA_MANAGED_OK || ruda_g_descriptor != &"
        ) && ruda_native_c_write_descriptor_name(
            buffer, layout.kind, entry->result_type
        ) && ruda_native_c_write(buffer,
            ") { ruda_g_exit = ruda_g_public_status(7, &ruda_g_p, &ruda_g_x.heap); goto ruda_g_public_root_cleanup; } if (ruda_g_variant == 0) { ruda_g_exit = 0; goto ruda_g_public_root_cleanup; } if (ruda_g_variant != 1 || ruda_managed_enum_get_payload(&ruda_g_x.heap, ruda_g_result, 1, 0, &ruda_g_cell) != RUDA_MANAGED_OK) { ruda_g_exit = ruda_g_public_status(7, &ruda_g_p, &ruda_g_x.heap); goto ruda_g_public_root_cleanup; } ruda_g_error = ruda_g_cell.reference; ruda_g_s = "
        ) && ruda_native_c_symbol(buffer, message->symbol)
        && ruda_native_c_write(buffer,
            "(&ruda_g_x, &ruda_g_p, ruda_g_error, &ruda_g_error_message, &ruda_g_start, &ruda_g_end); if (ruda_g_s != 0) ruda_g_exit = ruda_g_public_status(ruda_g_s, &ruda_g_p, &ruda_g_x.heap); else ruda_g_exit = ruda_g_public_message(\"error\", \"G4003\", ruda_g_error_message.data, ruda_g_error_message.length, 1); ruda_g_public_root_cleanup: if (ruda_managed_root_pop(&ruda_g_x.heap, &ruda_g_root_frame) != RUDA_MANAGED_OK) ruda_g_exit = ruda_g_public_status(7, &ruda_g_p, &ruda_g_x.heap); ruda_g_public_cleanup: ruda_g_execution_dispose(&ruda_g_x); return ruda_g_exit; }\n"
        );
}
