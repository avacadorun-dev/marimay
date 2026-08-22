#include "c_emitter_internal.h"

static const MarimayMirEnum *mmay_native_c_io_enum_by_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    if (function == NULL) return NULL;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_vector_at_const(
            &function->enums, index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

bool mmay_native_c_emit_io_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(buffer,
        "#ifndef MMAY_G_STDOUT_GRANTED\n"
        "#define MMAY_G_STDOUT_GRANTED 1\n"
        "#endif\n"
        "static bool mmay_g_host_stdout(void *context, const unsigned char *line, size_t length) { (void)context; return fwrite(line, 1, length, stdout) == length && fflush(stdout) == 0; }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, bool denied, MarimayManagedReference *out) { static const unsigned char capability_name[] = \"io.stdout\"; MarimayManagedReference capability = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&capability, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload; MarimayManagedStatus status, pop_status; memset(&payload, 0, sizeof(payload)); status = mmay_managed_root_push(&x->heap, &roots, locations, 2); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (denied) { status = mmay_managed_string_new(&x->heap, capability_name, sizeof(capability_name) - 1, &capability); if (status == MMAY_MANAGED_OK) { payload.reference = capability; status = mmay_managed_enum_new(&x->heap, error_descriptor, 0, &payload, 1, &error); } } else { status = mmay_managed_enum_new(&x->heap, error_descriptor, 1, NULL, 0, &error); } if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_println(mmay_g_execution *x, mmay_g_string text, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { MarimayManagedStatus status; MarimayTestOutputStatus output_status; MarimayIoStdout sink = {MMAY_G_STDOUT_GRANTED != 0, NULL, mmay_g_host_stdout}; unsigned char *line; size_t length; status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (!mmay_size_add(text.length, 1, &length)) return 6; line = mmay_allocate(&x->allocator, length); if (line == NULL) return 5; if (text.length != 0) memcpy(line, text.data, text.length); line[text.length] = '\\n'; output_status = mmay_test_output_publish(&x->test_output, &sink, line, length); mmay_deallocate(&x->allocator, line, length); if (output_status == MMAY_TEST_OUTPUT_OK) return 0; if (output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED) return mmay_g_io_error(x, result_descriptor, error_descriptor, true, out); if (output_status == MMAY_TEST_OUTPUT_WRITE_FAILED) return mmay_g_io_error(x, result_descriptor, error_descriptor, false, out); if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_test_output_begin(mmay_g_execution *x, mmay_g_string expected, bool *marker) { MarimayTestOutputStatus status = mmay_test_output_begin(&x->test_output, x->allocator, expected.data, expected.length); if (status == MMAY_TEST_OUTPUT_OK) { *marker = true; return 0; } if (status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
        "static int mmay_g_test_output_end(mmay_g_execution *x, mmay_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { MarimayTestOutputStatus status = mmay_test_output_end(&x->test_output); const MarimayOwnedBytes *failure; if (status == MMAY_TEST_OUTPUT_OK) return 0; if (status == MMAY_TEST_OUTPUT_MISMATCH) { if (p == NULL) return 4; failure = mmay_test_output_failure(&x->test_output); if (failure == NULL) return 7; if (failure->length > sizeof(p->message_storage)) return 6; if (failure->length != 0) memcpy(p->message_storage, failure->data, failure->length); p->code = \"G4004\"; p->message.data = p->message_storage; p->message.length = failure->length; p->message.managed = mmay_managed_invalid_reference(); p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return mmay_g_append_frame(p, symbol, start, end) ? 4 : 5; } if (status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
    );
}

bool mmay_native_c_emit_io_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnum *result = instruction != NULL
        ? mmay_vector_at_const(&function->enums, instruction->enum_id) : NULL;
    const MarimayMirEnumVariant *error_variant = result != NULL
        ? mmay_vector_at_const(
            &function->enum_variants, result->variant_start + 1
        ) : NULL;
    const MarimayMirEnumPayload *error_payload = error_variant != NULL
        ? mmay_vector_at_const(
            &function->enum_payloads, error_variant->payload_start
        ) : NULL;
    const MarimayMirEnum *error = error_payload != NULL
        ? mmay_native_c_io_enum_by_type(function, error_payload->type) : NULL;
    if (buffer == NULL || function == NULL || instruction == NULL
        || result == NULL || error_variant == NULL || error_payload == NULL
        || error == NULL) {
        return false;
    }
    if (!mmay_native_c_write(
            buffer,
            "  mmay_g_s = mmay_g_io_println(mmay_g_x, mmay_g_v_%u, &",
            instruction->first
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, result->type
        ) || !mmay_native_c_write(buffer, ", &")
        || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, error->type
        ) || !mmay_native_c_write(
            buffer,
            ", &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
            instruction->target, instruction->span.start,
            instruction->span.end
        )) {
        return false;
    }
    return true;
}

bool mmay_native_c_emit_test_output_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    if (buffer == NULL || function == NULL || instruction == NULL) {
        return false;
    }
    if (instruction->kind == MMAY_MIR_TEST_OUTPUT_BEGIN) {
        return mmay_native_c_write(
            buffer,
            "  mmay_g_s = mmay_g_test_output_begin(mmay_g_x, mmay_g_v_%u, &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
            instruction->first, instruction->target,
            instruction->span.start, instruction->span.end
        );
    }
    return mmay_native_c_write(
            buffer,
            "  if (!mmay_g_v_%u) { mmay_g_s = 7; *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; } mmay_g_s = mmay_g_test_output_end(mmay_g_x, mmay_g_p, ",
            instruction->first, instruction->span.start,
            instruction->span.end
        ) && mmay_native_c_symbol_data(buffer, function->symbol)
        && mmay_native_c_write(
            buffer,
            ", %zu, %zu); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
            instruction->span.start, instruction->span.end,
            instruction->span.start, instruction->span.end
        );
}
