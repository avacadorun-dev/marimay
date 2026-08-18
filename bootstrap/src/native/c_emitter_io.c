#include "c_emitter_internal.h"

static const RudaMirEnum *ruda_native_c_io_enum_by_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    if (function == NULL) return NULL;
    for (index = 0; index < function->enums.length; index += 1) {
        const RudaMirEnum *layout = ruda_vector_at_const(
            &function->enums, index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

bool ruda_native_c_emit_io_runtime(RudaNativeCBuffer *buffer) {
    return ruda_native_c_write(buffer,
        "#ifndef RUDA_G_STDOUT_GRANTED\n"
        "#define RUDA_G_STDOUT_GRANTED 1\n"
        "#endif\n"
        "static bool ruda_g_host_stdout(void *context, const unsigned char *line, size_t length) { (void)context; return fwrite(line, 1, length, stdout) == length && fflush(stdout) == 0; }\n"
    ) && ruda_native_c_write(buffer,
        "static int ruda_g_io_error(ruda_g_execution *x, const RudaManagedEnumDescriptor *result_descriptor, const RudaManagedEnumDescriptor *error_descriptor, bool denied, RudaManagedReference *out) { static const unsigned char capability_name[] = \"io.stdout\"; RudaManagedReference capability = ruda_managed_invalid_reference(), error = ruda_managed_invalid_reference(); RudaManagedReference *locations[2] = {&capability, &error}; RudaManagedRootFrame roots = {0}; RudaManagedCell payload; RudaManagedStatus status, pop_status; memset(&payload, 0, sizeof(payload)); status = ruda_managed_root_push(&x->heap, &roots, locations, 2); if (status != RUDA_MANAGED_OK) return ruda_g_managed_status(status); if (denied) { status = ruda_managed_string_new(&x->heap, capability_name, sizeof(capability_name) - 1, &capability); if (status == RUDA_MANAGED_OK) { payload.reference = capability; status = ruda_managed_enum_new(&x->heap, error_descriptor, 0, &payload, 1, &error); } } else { status = ruda_managed_enum_new(&x->heap, error_descriptor, 1, NULL, 0, &error); } if (status == RUDA_MANAGED_OK) { payload.reference = error; status = ruda_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = ruda_managed_root_pop(&x->heap, &roots); if (status == RUDA_MANAGED_OK) status = pop_status; return ruda_g_managed_status(status); }\n"
    ) && ruda_native_c_write(buffer,
        "static int ruda_g_io_println(ruda_g_execution *x, ruda_g_string text, const RudaManagedEnumDescriptor *result_descriptor, const RudaManagedEnumDescriptor *error_descriptor, RudaManagedReference *out) { RudaManagedStatus status; RudaTestOutputStatus output_status; RudaIoStdout sink = {RUDA_G_STDOUT_GRANTED != 0, NULL, ruda_g_host_stdout}; unsigned char *line; size_t length; status = ruda_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); if (status != RUDA_MANAGED_OK) return ruda_g_managed_status(status); if (!ruda_size_add(text.length, 1, &length)) return 6; line = ruda_allocate(&x->allocator, length); if (line == NULL) return 5; if (text.length != 0) memcpy(line, text.data, text.length); line[text.length] = '\\n'; output_status = ruda_test_output_publish(&x->test_output, &sink, line, length); ruda_deallocate(&x->allocator, line, length); if (output_status == RUDA_TEST_OUTPUT_OK) return 0; if (output_status == RUDA_TEST_OUTPUT_CAPABILITY_DENIED) return ruda_g_io_error(x, result_descriptor, error_descriptor, true, out); if (output_status == RUDA_TEST_OUTPUT_WRITE_FAILED) return ruda_g_io_error(x, result_descriptor, error_descriptor, false, out); if (output_status == RUDA_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == RUDA_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
    ) && ruda_native_c_write(buffer,
        "static int ruda_g_test_output_begin(ruda_g_execution *x, ruda_g_string expected, bool *marker) { RudaTestOutputStatus status = ruda_test_output_begin(&x->test_output, x->allocator, expected.data, expected.length); if (status == RUDA_TEST_OUTPUT_OK) { *marker = true; return 0; } if (status == RUDA_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (status == RUDA_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
        "static int ruda_g_test_output_end(ruda_g_execution *x, ruda_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { RudaTestOutputStatus status = ruda_test_output_end(&x->test_output); const RudaOwnedBytes *failure; if (status == RUDA_TEST_OUTPUT_OK) return 0; if (status == RUDA_TEST_OUTPUT_MISMATCH) { if (p == NULL) return 4; failure = ruda_test_output_failure(&x->test_output); if (failure == NULL) return 7; if (failure->length > sizeof(p->message_storage)) return 6; if (failure->length != 0) memcpy(p->message_storage, failure->data, failure->length); p->code = \"G4004\"; p->message.data = p->message_storage; p->message.length = failure->length; p->message.managed = ruda_managed_invalid_reference(); p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return ruda_g_append_frame(p, symbol, start, end) ? 4 : 5; } if (status == RUDA_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (status == RUDA_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
    );
}

bool ruda_native_c_emit_io_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirEnum *result = instruction != NULL
        ? ruda_vector_at_const(&function->enums, instruction->enum_id) : NULL;
    const RudaMirEnumVariant *error_variant = result != NULL
        ? ruda_vector_at_const(
            &function->enum_variants, result->variant_start + 1
        ) : NULL;
    const RudaMirEnumPayload *error_payload = error_variant != NULL
        ? ruda_vector_at_const(
            &function->enum_payloads, error_variant->payload_start
        ) : NULL;
    const RudaMirEnum *error = error_payload != NULL
        ? ruda_native_c_io_enum_by_type(function, error_payload->type) : NULL;
    if (buffer == NULL || function == NULL || instruction == NULL
        || result == NULL || error_variant == NULL || error_payload == NULL
        || error == NULL) {
        return false;
    }
    if (!ruda_native_c_write(
            buffer,
            "  ruda_g_s = ruda_g_io_println(ruda_g_x, ruda_g_v_%u, &",
            instruction->first
        ) || !ruda_native_c_write_descriptor_name(
            buffer, RUDA_NATIVE_C_MANAGED_ENUM, result->type
        ) || !ruda_native_c_write(buffer, ", &")
        || !ruda_native_c_write_descriptor_name(
            buffer, RUDA_NATIVE_C_MANAGED_ENUM, error->type
        ) || !ruda_native_c_write(
            buffer,
            ", &ruda_g_v_%u); if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; }\n",
            instruction->target, instruction->span.start,
            instruction->span.end
        )) {
        return false;
    }
    return true;
}

bool ruda_native_c_emit_test_output_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    if (buffer == NULL || function == NULL || instruction == NULL) {
        return false;
    }
    if (instruction->kind == RUDA_MIR_TEST_OUTPUT_BEGIN) {
        return ruda_native_c_write(
            buffer,
            "  ruda_g_s = ruda_g_test_output_begin(ruda_g_x, ruda_g_v_%u, &ruda_g_v_%u); if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; }\n",
            instruction->first, instruction->target,
            instruction->span.start, instruction->span.end
        );
    }
    return ruda_native_c_write(
            buffer,
            "  if (!ruda_g_v_%u) { ruda_g_s = 7; *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; } ruda_g_s = ruda_g_test_output_end(ruda_g_x, ruda_g_p, ",
            instruction->first, instruction->span.start,
            instruction->span.end
        ) && ruda_native_c_symbol_data(buffer, function->symbol)
        && ruda_native_c_write(
            buffer,
            ", %zu, %zu); if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; }\n",
            instruction->span.start, instruction->span.end,
            instruction->span.start, instruction->span.end
        );
}
