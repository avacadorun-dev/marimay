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
        "#if defined(__clang__) || defined(__GNUC__)\n"
        "#define MMAY_G_UNUSED __attribute__((unused))\n"
        "#else\n"
        "#define MMAY_G_UNUSED\n"
        "#endif\n"
        "static bool mmay_g_host_stdout(void *context, const unsigned char *line, size_t length) { (void)context; return fwrite(line, 1, length, stdout) == length && fflush(stdout) == 0; }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, bool denied, MarimayManagedReference *out) { static const unsigned char capability_name[] = \"io.stdout\"; MarimayManagedReference capability = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&capability, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload; MarimayManagedStatus status, pop_status; memset(&payload, 0, sizeof(payload)); status = mmay_managed_root_push(&x->heap, &roots, locations, 2); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (denied) { status = mmay_managed_string_new(&x->heap, capability_name, sizeof(capability_name) - 1, &capability); if (status == MMAY_MANAGED_OK) { payload.reference = capability; status = mmay_managed_enum_new(&x->heap, error_descriptor, 0, &payload, 1, &error); } } else { status = mmay_managed_enum_new(&x->heap, error_descriptor, 2, NULL, 0, &error); } if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_read_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayIoStatus io_status, size_t limit, MarimayManagedReference *out) { static const unsigned char capability_name[] = \"io.stdin\"; size_t variant = io_status == MMAY_IO_CAPABILITY_DENIED ? 0 : io_status == MMAY_IO_INVALID_UTF8 ? 4 : io_status == MMAY_IO_LINE_TOO_LONG ? 5 : 1; MarimayManagedReference capability = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&capability, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status, pop_status; status = mmay_managed_root_push(&x->heap, &roots, locations, 2); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (variant == 0) { status = mmay_managed_string_new(&x->heap, capability_name, sizeof(capability_name) - 1, &capability); if (status == MMAY_MANAGED_OK) { payload.reference = capability; status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, &payload, 1, &error); } } else { if (variant == 5) payload.f64_bits = (uint64_t)limit; status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, variant == 5 ? &payload : NULL, variant == 5 ? 1 : 0, &error); } if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_println(mmay_g_execution *x, mmay_g_string text, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { MarimayManagedStatus status; MarimayTestOutputStatus output_status; MarimayIoStdout sink = {MMAY_G_STDOUT_GRANTED != 0, NULL, mmay_g_host_stdout, false, NULL, NULL, NULL, NULL, false, NULL, NULL}; unsigned char *line; size_t length; status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (!mmay_size_add(text.length, 1, &length)) return 6; line = mmay_allocate(&x->allocator, length); if (line == NULL) return 5; if (text.length != 0) memcpy(line, text.data, text.length); line[text.length] = '\\n'; output_status = mmay_test_output_publish(&x->test_output, &sink, line, length); mmay_deallocate(&x->allocator, line, length); if (output_status == MMAY_TEST_OUTPUT_OK) return 0; if (output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED) return mmay_g_io_error(x, result_descriptor, error_descriptor, true, out); if (output_status == MMAY_TEST_OUTPUT_WRITE_FAILED) return mmay_g_io_error(x, result_descriptor, error_descriptor, false, out); if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
    ) && mmay_native_c_write(buffer,
        "static MMAY_G_UNUSED int mmay_g_io_cp32(mmay_g_execution *x, mmay_g_string text, int operation, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { bool stderr_stream = operation == 2 || operation == 3 || operation == 5; bool newline = operation == 1 || operation == 3; FILE *stream = stderr_stream ? stderr : stdout; MarimayManagedStatus status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (operation >= 4) return fflush(stream) == 0 ? 0 : mmay_g_io_error(x, result_descriptor, error_descriptor, false, out); if (!stderr_stream && mmay_test_output_depth(&x->test_output) != 0) { MarimayIoStdout sink = {MMAY_G_STDOUT_GRANTED != 0, NULL, mmay_g_host_stdout, false, NULL, NULL, NULL, NULL, false, NULL, NULL}; MarimayTestOutputStatus output_status; size_t length = text.length + (newline ? 1 : 0); unsigned char *line = length == 0 ? NULL : mmay_allocate(&x->allocator, length); if (length != 0 && line == NULL) return 5; if (text.length != 0) memcpy(line, text.data, text.length); if (newline) line[text.length] = '\\n'; output_status = mmay_test_output_publish(&x->test_output, &sink, line, length); if (line != NULL) mmay_deallocate(&x->allocator, line, length); if (output_status == MMAY_TEST_OUTPUT_OK) return 0; if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return mmay_g_io_error(x, result_descriptor, error_descriptor, output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED, out); } if ((text.length != 0 && fwrite(text.data, 1, text.length, stream) != text.length) || (newline && fputc('\\n', stream) == EOF)) return mmay_g_io_error(x, result_descriptor, error_descriptor, false, out); return 0; }\n"
    ) && mmay_native_c_write(buffer,
        "static bool mmay_g_stdin_read(void *context, unsigned char *destination, size_t capacity, size_t *count) { int byte; (void)context; if (destination == NULL || count == NULL || capacity == 0) return false; byte = fgetc(stdin); if (byte == EOF) { *count = 0; return !ferror(stdin); } destination[0] = (unsigned char)byte; *count = 1; return true; }\n"
        "static MMAY_G_UNUSED int mmay_g_io_read_line(mmay_g_execution *x, int64_t limit, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedEnumDescriptor *option_descriptor, MarimayManagedReference *out) { MarimayIoInput input = {true, NULL, mmay_g_stdin_read}; MarimayOwnedBytes line = {0}; MarimayManagedReference string = mmay_managed_invalid_reference(), option = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[3] = {&string, &option, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status; MarimayIoStatus io_status; bool eof = false; if (limit < 0 || (uint64_t)limit > MMAY_IO_MAX_LINE_BYTES) return mmay_g_io_read_error(x, result_descriptor, error_descriptor, MMAY_IO_LINE_TOO_LONG, limit < 0 ? 0 : (size_t)limit, out); io_status = mmay_io_read_line(&input, x->allocator, (size_t)limit, &line, &eof); if (io_status != MMAY_IO_OK) return mmay_g_io_read_error(x, result_descriptor, error_descriptor, io_status, (size_t)limit, out); status = mmay_managed_root_push(&x->heap, &roots, locations, 3); if (status == MMAY_MANAGED_OK && !eof) status = mmay_managed_string_new(&x->heap, line.data, line.length, &string); if (status == MMAY_MANAGED_OK) { payload.reference = string; status = mmay_managed_enum_new(&x->heap, option_descriptor, eof ? 1 : 0, eof ? NULL : &payload, eof ? 0 : 1, &option); } if (status == MMAY_MANAGED_OK) { payload.reference = option; status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, &payload, 1, out); } mmay_owned_bytes_dispose(&line); { MarimayManagedStatus pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; } return mmay_g_managed_status(status); }\n"
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
    const MarimayMirEnum *option = instruction != NULL
        ? mmay_vector_at_const(&function->enums, instruction->option_enum_id)
        : NULL;
    if (buffer == NULL || function == NULL || instruction == NULL
        || result == NULL || error_variant == NULL || error_payload == NULL
        || error == NULL || (instruction->kind == MMAY_MIR_IO_READ_LINE
            && option == NULL)) {
        return false;
    }
    if (!(instruction->kind == MMAY_MIR_IO_READ_LINE
            ? mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_io_read_line(mmay_g_x, mmay_g_v_%u, &",
                instruction->first
            )
            : instruction->kind == MMAY_MIR_IO_FLUSH
            ? mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_io_cp32(mmay_g_x, (mmay_g_string){NULL, 0, mmay_managed_invalid_reference()}, %d, &",
                (int)instruction->io_operation
            )
            : instruction->kind == MMAY_MIR_IO_WRITE
            ? mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_io_cp32(mmay_g_x, mmay_g_v_%u, %d, &",
                instruction->first, (int)instruction->io_operation
            )
            : mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_io_println(mmay_g_x, mmay_g_v_%u, &",
                instruction->first
            )) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, result->type
        ) || !mmay_native_c_write(buffer, ", &")
        || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, error->type
        ) || (instruction->kind == MMAY_MIR_IO_READ_LINE
            && (!mmay_native_c_write(buffer, ", &")
                || !mmay_native_c_write_descriptor_name(
                    buffer, MMAY_NATIVE_C_MANAGED_ENUM, option->type
                ))) || !mmay_native_c_write(
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
