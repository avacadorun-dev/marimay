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
    const MarimayMirEnumPayload *ok_payload = NULL;
    const MarimayMirEnumPayload *payload = NULL;
    const MarimayMirFunction *message = NULL;
    const MarimayTypeId *parameter_type = NULL;
    const MarimayTypeId *entry_parameter = NULL;
    const MarimayMirArray *argument_array = NULL;
    if (entry == NULL || functions == NULL || options == NULL) {
        return MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT;
    }
    if (options->executable_takes_arguments) {
        entry_parameter = mmay_vector_at_const(&entry->register_types, 0);
        argument_array = entry_parameter != NULL
            ? mmay_native_c_array(entry, *entry_parameter) : NULL;
        if (entry->parameter_count != 1 || entry_parameter == NULL
            || argument_array == NULL
            || argument_array->element_type != MMAY_TYPE_STRING) {
            return MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        }
    } else if (entry->parameter_count != 0) {
        return MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    if (!options->executable_returns_result) {
        return entry->result_type == (options->executable_returns_status
                ? MMAY_TYPE_I64 : MMAY_TYPE_VOID)
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
    ok_payload = ok == NULL || ok->payload_count == 0 ? NULL
        : mmay_vector_at_const(
            &layout.function->enum_payloads, ok->payload_start
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
        || ok == NULL
        || ok->payload_count != (options->executable_returns_status ? 1U : 0U)
        || (options->executable_returns_status
            && (ok_payload == NULL || ok_payload->type != MMAY_TYPE_I64))
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
        "static int mmay_g_public_argument_status(MarimayProgramArgumentsStatus status) { static const unsigned char utf8[] = \"program argument is not valid UTF-8\"; static const unsigned char nul[] = \"program argument contains NUL\"; static const unsigned char count[] = \"program argument count exceeds 4096\"; static const unsigned char bytes[] = \"program arguments exceed the 1048576-byte total or 262144-byte per-argument limit\"; static const unsigned char invalid[] = \"program arguments failed validation\"; static const unsigned char allocation[] = \"runtime host allocation failed\"; if (status == MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED) return mmay_g_public_message(\"error\", \"I0002\", allocation, sizeof(allocation) - 1, 70); if (status == MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8) return mmay_g_public_message(\"error\", \"G4006\", utf8, sizeof(utf8) - 1, 1); if (status == MMAY_PROGRAM_ARGUMENTS_CONTAINS_NUL) return mmay_g_public_message(\"error\", \"G4006\", nul, sizeof(nul) - 1, 1); if (status == MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT) return mmay_g_public_message(\"error\", \"G4006\", count, sizeof(count) - 1, 1); if (status == MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT) return mmay_g_public_message(\"error\", \"G4006\", bytes, sizeof(bytes) - 1, 1); return mmay_g_public_message(\"error\", \"G4006\", invalid, sizeof(invalid) - 1, 1); }\n"
        "static int mmay_g_public_selected_status(int64_t status) { static const unsigned char invalid[] = \"program exit status is outside 0..255\"; return status >= 0 && status <= 255 ? (int)status : mmay_g_public_message(\"error\", \"G4007\", invalid, sizeof(invalid) - 1, 1); }\n"
        "static int mmay_g_public_budget(const MarimayManagedHeap *heap) { MarimayManagedResourceFailure failure; if (heap == NULL || !mmay_managed_heap_last_resource_failure(heap, &failure) || failure.classification != MMAY_MANAGED_RESOURCE_FAILURE_BUDGET) return 0; if (fprintf(stderr, \"error[I0002]: managed heap budget exceeded: live=%%zu bytes, requested=%%zu bytes, limit=%%zu bytes\\n\", failure.live_bytes, failure.requested_bytes, failure.limit_bytes) < 0 || fflush(stderr) != 0) return 70; return 70; }\n"
        "static int mmay_g_public_status(int status, const mmay_g_panic *panic, const MarimayManagedHeap *heap) { static const unsigned char overflow[] = \"integer overflow\"; static const unsigned char division[] = \"integer division by zero\"; static const unsigned char shift[] = \"integer shift count is outside width\"; static const unsigned char allocation[] = \"runtime host allocation failed\"; static const unsigned char limit[] = \"runtime resource limit reached\"; static const unsigned char integrity[] = \"native runtime integrity failure\"; int budget = 0; if (status == 4 && panic != NULL) return mmay_g_public_message(\"panic\", panic->code, panic->message.data, panic->message.length, 1); if (status == 1) return mmay_g_public_message(\"panic\", \"G4001\", overflow, sizeof(overflow) - 1, 1); if (status == 2) return mmay_g_public_message(\"panic\", \"G4001\", division, sizeof(division) - 1, 1); if (status == 3) return mmay_g_public_message(\"panic\", \"G4001\", shift, sizeof(shift) - 1, 1); if (status == 5) return mmay_g_public_message(\"error\", \"I0002\", allocation, sizeof(allocation) - 1, 70); if (status == 6) { budget = mmay_g_public_budget(heap); if (budget != 0) return budget; return mmay_g_public_message(\"error\", \"I0002\", limit, sizeof(limit) - 1, 70); } return mmay_g_public_message(\"error\", \"I0002\", integrity, sizeof(integrity) - 1, 70); }\n\n"
    );
}

bool mmay_native_c_emit_executable_host_open(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(
        buffer,
        "#if defined(_WIN32)\nint wmain(int mmay_g_argc, wchar_t **mmay_g_argv) {\n#else\nint main(int mmay_g_argc, char **mmay_g_argv) {\n#endif\nsize_t mmay_g_start = 0, mmay_g_end = 0; int mmay_g_s, mmay_g_exit = 70; static mmay_g_frame mmay_g_frames[MMAY_G_MAX_FRAMES]; static mmay_g_suppressed_panic mmay_g_suppressed[MMAY_G_MAX_SUPPRESSED]; mmay_g_panic mmay_g_p = {0}; mmay_g_execution mmay_g_x = {0}; MarimayManagedHeapOptions mmay_g_heap_options; MarimayHostArguments mmay_g_host_arguments = {0}; MarimayProgramArgumentList mmay_g_argument_list = {0}; MarimayManagedReference mmay_g_arguments = mmay_managed_invalid_reference_generated(); MarimayProgramArgumentsStatus mmay_g_argument_status; MarimayManagedStatus mmay_g_argument_managed_status; (void)mmay_g_arguments; (void)mmay_g_argument_managed_status; (void)&mmay_g_public_selected_status; "
    );
}

bool mmay_native_c_emit_executable_host_arguments(
    MarimayNativeCBuffer *buffer
) {
    return mmay_native_c_write(
        buffer,
        "if (mmay_g_argc < 1) { mmay_g_exit = mmay_g_public_argument_status(MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT); goto mmay_g_public_host_cleanup; } mmay_g_argument_status = mmay_host_arguments_import(mmay_g_x.allocator, mmay_g_argc > 1 ? (const MarimayHostArgumentChar *const *)(mmay_g_argv + 1) : NULL, mmay_g_argc > 1 ? (size_t)(mmay_g_argc - 1) : 0, MMAY_PROGRAM_ARGUMENT_MAX_COUNT, MMAY_PROGRAM_ARGUMENT_MAX_BYTES, &mmay_g_host_arguments); if (mmay_g_argument_status != MMAY_PROGRAM_ARGUMENTS_OK) { mmay_g_exit = mmay_g_public_argument_status(mmay_g_argument_status); goto mmay_g_public_host_cleanup; } mmay_g_argument_list.values = mmay_g_host_arguments.views; mmay_g_argument_list.count = mmay_g_host_arguments.count; (void)mmay_g_argument_list; "
    );
}

bool mmay_native_c_emit_executable_argument_value(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *entry
) {
    const MarimayTypeId *argument_type = entry != NULL
        ? mmay_vector_at_const(&entry->register_types, 0) : NULL;
    return argument_type != NULL && mmay_native_c_write(
        buffer,
        "mmay_g_argument_managed_status = mmay_program_arguments_managed(&mmay_g_x.heap, %s, mmay_g_argument_list, &mmay_g_arguments); if (mmay_g_argument_managed_status != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_status(mmay_g_managed_status(mmay_g_argument_managed_status), &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup; } ",
        mmay_native_c_array_descriptor(entry, *argument_type)
    );
}

bool mmay_native_c_emit_executable_host_close(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(
        buffer,
        "mmay_g_public_cleanup: mmay_g_execution_dispose(&mmay_g_x); mmay_g_public_host_cleanup: mmay_host_arguments_dispose(&mmay_g_host_arguments); return mmay_g_exit; }\n"
    );
}

static bool mmay_native_c_emit_entry_call(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *entry,
    const MarimayNativeCEmitOptions *options,
    const char *output
) {
    return mmay_native_c_write(buffer, "mmay_g_s = ")
        && mmay_native_c_symbol(buffer, entry->symbol)
        && mmay_native_c_write(buffer, "(&mmay_g_x, &mmay_g_p")
        && (!options->executable_takes_arguments
            || mmay_native_c_write(buffer, ", mmay_g_arguments"))
        && (output == NULL || mmay_native_c_write(buffer, ", &%s", output))
        && mmay_native_c_write(
            buffer, ", &mmay_g_start, &mmay_g_end); "
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
        if (options->executable_returns_status) {
            return mmay_native_c_write(buffer, "int64_t mmay_g_result = 0; ")
                && mmay_native_c_emit_entry_call(
                    buffer, entry, options, "mmay_g_result"
                ) && mmay_native_c_write(buffer,
                    "mmay_g_exit = mmay_g_s == 0 ? mmay_g_public_selected_status(mmay_g_result) : mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup;\n"
                );
        }
        return mmay_native_c_emit_entry_call(buffer, entry, options, NULL)
            && mmay_native_c_write(buffer,
                "mmay_g_exit = mmay_g_s == 0 ? 0 : mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup;\n"
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
            "MarimayManagedReference mmay_g_result = mmay_managed_invalid_reference_generated(), mmay_g_error = mmay_managed_invalid_reference_generated(); MarimayManagedReference *mmay_g_roots[2] = {&mmay_g_result, &mmay_g_error}; MarimayManagedRootFrame mmay_g_root_frame = {0}; const MarimayManagedEnumDescriptor *mmay_g_descriptor = NULL; size_t mmay_g_variant = 0; MarimayManagedCell mmay_g_cell; mmay_g_string mmay_g_error_message = {0}; "
        ) && mmay_native_c_emit_entry_call(
            buffer, entry, options, "mmay_g_result"
        ) && mmay_native_c_write(buffer,
            "if (mmay_g_s != 0) { mmay_g_exit = mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup; } if (mmay_managed_root_push(&mmay_g_x.heap, &mmay_g_root_frame, mmay_g_roots, 2) != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup; } if (mmay_managed_enum_describe(&mmay_g_x.heap, mmay_g_result, &mmay_g_descriptor, &mmay_g_variant) != MMAY_MANAGED_OK || mmay_g_descriptor != &"
        ) && mmay_native_c_write_descriptor_name(
            buffer, layout.kind, entry->result_type
        ) && mmay_native_c_write(buffer,
            ") { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_root_cleanup; } if (mmay_g_variant == 0) { "
        ) && (!options->executable_returns_status || mmay_native_c_write(
            buffer,
            "if (mmay_managed_enum_get_payload(&mmay_g_x.heap, mmay_g_result, 0, 0, &mmay_g_cell) != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_root_cleanup; } mmay_g_exit = mmay_g_public_selected_status(mmay_i64_from_twos_complement_fast(mmay_g_cell.f64_bits)); "
        )) && (options->executable_returns_status || mmay_native_c_write(
            buffer, "mmay_g_exit = 0; "
        )) && mmay_native_c_write(buffer,
            "goto mmay_g_public_root_cleanup; } if (mmay_g_variant != 1 || mmay_managed_enum_get_payload(&mmay_g_x.heap, mmay_g_result, 1, 0, &mmay_g_cell) != MMAY_MANAGED_OK) { mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_root_cleanup; } mmay_g_error = mmay_g_cell.reference; mmay_g_s = "
        ) && mmay_native_c_symbol(buffer, message->symbol)
        && mmay_native_c_write(buffer,
            "(&mmay_g_x, &mmay_g_p, mmay_g_error, &mmay_g_error_message, &mmay_g_start, &mmay_g_end); if (mmay_g_s != 0) mmay_g_exit = mmay_g_public_status(mmay_g_s, &mmay_g_p, &mmay_g_x.heap); else mmay_g_exit = mmay_g_public_message(\"error\", \"G4003\", mmay_g_error_message.data, mmay_g_error_message.length, 1); mmay_g_public_root_cleanup: if (mmay_managed_root_pop(&mmay_g_x.heap, &mmay_g_root_frame) != MMAY_MANAGED_OK) mmay_g_exit = mmay_g_public_status(7, &mmay_g_p, &mmay_g_x.heap); goto mmay_g_public_cleanup;\n"
        );
}
