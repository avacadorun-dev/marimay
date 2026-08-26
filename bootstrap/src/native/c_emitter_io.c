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
        "#ifndef MMAY_G_STDERR_GRANTED\n"
        "#define MMAY_G_STDERR_GRANTED 1\n"
        "#endif\n"
        "#ifndef MMAY_G_STDIN_GRANTED\n"
        "#define MMAY_G_STDIN_GRANTED 1\n"
        "#endif\n"
        "#if defined(__clang__) || defined(__GNUC__)\n"
        "#define MMAY_G_UNUSED __attribute__((unused))\n"
        "#else\n"
        "#define MMAY_G_UNUSED\n"
        "#endif\n"
        "static bool mmay_g_host_stdout(void *context, const unsigned char *line, size_t length) { (void)context; return fwrite(line, 1, length, stdout) == length && fflush(stdout) == 0; }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, bool denied, const unsigned char *capability_name, size_t capability_length, size_t failure_variant, MarimayManagedReference *out) { MarimayManagedReference capability = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&capability, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status, pop_status; status = mmay_managed_root_push(&x->heap, &roots, locations, 2); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (denied) { status = mmay_managed_string_new(&x->heap, capability_name, capability_length, &capability); if (status == MMAY_MANAGED_OK) { payload.reference = capability; status = mmay_managed_enum_new(&x->heap, error_descriptor, 0, &payload, 1, &error); } } else { status = mmay_managed_enum_new(&x->heap, error_descriptor, failure_variant, NULL, 0, &error); } if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static MMAY_G_UNUSED int mmay_g_io_read_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayIoStatus io_status, int64_t limit, MarimayManagedReference *out) { static const unsigned char capability_name[] = \"io.stdin\"; size_t variant = io_status == MMAY_IO_CAPABILITY_DENIED ? 0 : io_status == MMAY_IO_INVALID_UTF8 ? 4 : io_status == MMAY_IO_LINE_TOO_LONG ? 5 : io_status == MMAY_IO_INPUT_TOO_LONG ? 6 : 1; MarimayManagedReference capability = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&capability, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status, pop_status; status = mmay_managed_root_push(&x->heap, &roots, locations, 2); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (variant == 0) { status = mmay_managed_string_new(&x->heap, capability_name, sizeof(capability_name) - 1, &capability); if (status == MMAY_MANAGED_OK) { payload.reference = capability; status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, &payload, 1, &error); } } else { if (variant == 5 || variant == 6) payload.f64_bits = (uint64_t)limit; status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, variant == 5 || variant == 6 ? &payload : NULL, variant == 5 || variant == 6 ? 1 : 0, &error); } if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_io_println(mmay_g_execution *x, mmay_g_string text, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { static const unsigned char capability_name[] = \"io.stdout\"; MarimayManagedStatus status; MarimayTestOutputStatus output_status; MarimayIoStdout sink = {0}; unsigned char *line; size_t length; sink.capability_granted = MMAY_G_STDOUT_GRANTED != 0; sink.publish = mmay_g_host_stdout; status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (!mmay_size_add(text.length, 1, &length)) return 6; line = mmay_allocate(&x->allocator, length); if (line == NULL) return 5; if (text.length != 0) memcpy(line, text.data, text.length); line[text.length] = '\\n'; output_status = mmay_test_output_publish(&x->test_output, &sink, line, length); mmay_deallocate(&x->allocator, line, length); if (output_status == MMAY_TEST_OUTPUT_OK) return 0; if (output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED) return mmay_g_io_error(x, result_descriptor, error_descriptor, true, capability_name, sizeof(capability_name) - 1, 1, out); if (output_status == MMAY_TEST_OUTPUT_WRITE_FAILED) return mmay_g_io_error(x, result_descriptor, error_descriptor, false, capability_name, sizeof(capability_name) - 1, 1, out); if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return 7; }\n"
    ) && mmay_native_c_write(buffer,
        "static MarimayIoTransferStatus mmay_g_file_write(void *context, const unsigned char *bytes, size_t length, size_t *count) { FILE *stream = context; size_t amount; if (stream == NULL || count == NULL || (bytes == NULL && length != 0)) return MMAY_IO_TRANSFER_FAILED; errno = 0; amount = fwrite(bytes, 1, length, stream); *count = amount; if (amount != 0 || length == 0) return MMAY_IO_TRANSFER_OK; if (ferror(stream) && errno == EINTR) { clearerr(stream); return MMAY_IO_TRANSFER_RETRY; } return MMAY_IO_TRANSFER_FAILED; }\n"
        "static MarimayIoTransferStatus mmay_g_file_flush(void *context) { FILE *stream = context; if (stream == NULL) return MMAY_IO_TRANSFER_FAILED; errno = 0; if (fflush(stream) == 0) return MMAY_IO_TRANSFER_OK; if (errno == EINTR) { clearerr(stream); return MMAY_IO_TRANSFER_RETRY; } return MMAY_IO_TRANSFER_FAILED; }\n"
        "static MMAY_G_UNUSED int mmay_g_io_cp32(mmay_g_execution *x, mmay_g_string text, int operation, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { static const unsigned char stdout_name[] = \"io.stdout\", stderr_name[] = \"io.stderr\"; bool stderr_stream = operation == 2 || operation == 3 || operation == 5; bool newline = operation == 1 || operation == 3; const unsigned char *capability_name = stderr_stream ? stderr_name : stdout_name; size_t capability_length = stderr_stream ? sizeof(stderr_name) - 1 : sizeof(stdout_name) - 1; FILE *stream = stderr_stream ? stderr : stdout; MarimayIoOutput output = {stderr_stream ? MMAY_G_STDERR_GRANTED != 0 : MMAY_G_STDOUT_GRANTED != 0, stream, mmay_g_file_write, mmay_g_file_flush}; MarimayIoStatus io_status; MarimayManagedStatus status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); unsigned char *line = NULL; size_t length = text.length; if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (operation >= 4) { io_status = mmay_io_flush(&output); return io_status == MMAY_IO_OK ? 0 : mmay_g_io_error(x, result_descriptor, error_descriptor, io_status == MMAY_IO_CAPABILITY_DENIED, capability_name, capability_length, 3, out); } if (newline) { if (!mmay_size_add(text.length, 1, &length)) return 6; line = mmay_allocate(&x->allocator, length); if (line == NULL) return 5; if (text.length != 0) memcpy(line, text.data, text.length); line[text.length] = '\\n'; } if (length != 0 && !stderr_stream && mmay_test_output_depth(&x->test_output) != 0) { MarimayIoStdout sink = {0}; MarimayTestOutputStatus output_status; sink.capability_granted = MMAY_G_STDOUT_GRANTED != 0; sink.publish = mmay_g_host_stdout; output_status = mmay_test_output_publish(&x->test_output, &sink, newline ? line : text.data, length); if (line != NULL) mmay_deallocate(&x->allocator, line, length); if (output_status == MMAY_TEST_OUTPUT_OK) return 0; if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return mmay_g_io_error(x, result_descriptor, error_descriptor, output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED, capability_name, capability_length, 2, out); } io_status = mmay_io_write(&output, newline ? line : text.data, length); if (line != NULL) mmay_deallocate(&x->allocator, line, length); return io_status == MMAY_IO_OK ? 0 : mmay_g_io_error(x, result_descriptor, error_descriptor, io_status == MMAY_IO_CAPABILITY_DENIED, capability_name, capability_length, 2, out); }\n"
    ) && mmay_native_c_write(buffer,
        "static MarimayIoTransferStatus mmay_g_stdin_read(void *context, unsigned char *destination, size_t capacity, size_t *count) { FILE *stream = context; size_t amount; if (stream == NULL || destination == NULL || count == NULL || capacity == 0) return MMAY_IO_TRANSFER_FAILED; errno = 0; amount = fread(destination, 1, capacity, stream); *count = amount; if (amount != 0) return MMAY_IO_TRANSFER_OK; if (feof(stream)) return MMAY_IO_TRANSFER_END; if (ferror(stream) && errno == EINTR) { clearerr(stream); return MMAY_IO_TRANSFER_RETRY; } return MMAY_IO_TRANSFER_FAILED; }\n"
        "static MMAY_G_UNUSED int mmay_g_io_read_line(mmay_g_execution *x, int64_t limit, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedEnumDescriptor *option_descriptor, MarimayManagedReference *out) { MarimayIoInput input = {MMAY_G_STDIN_GRANTED != 0, stdin, mmay_g_stdin_read}; MarimayOwnedBytes line = {0}; MarimayManagedReference string = mmay_managed_invalid_reference(), option = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[3] = {&string, &option, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status; MarimayIoStatus io_status; bool eof = false; if (limit < 0 || (uint64_t)limit > MMAY_IO_MAX_LINE_BYTES) return mmay_g_io_read_error(x, result_descriptor, error_descriptor, MMAY_IO_LINE_TOO_LONG, limit, out); io_status = mmay_io_read_line(&input, x->allocator, (size_t)limit, &line, &eof); if (io_status == MMAY_IO_ALLOCATION_FAILED) return 5; if (io_status == MMAY_IO_INVALID_ARGUMENT) return 7; if (io_status != MMAY_IO_OK) return mmay_g_io_read_error(x, result_descriptor, error_descriptor, io_status, limit, out); status = mmay_managed_root_push(&x->heap, &roots, locations, 3); if (status == MMAY_MANAGED_OK && !eof) status = mmay_managed_string_new(&x->heap, line.data, line.length, &string); if (status == MMAY_MANAGED_OK) { payload.reference = string; status = mmay_managed_enum_new(&x->heap, option_descriptor, eof ? 1 : 0, eof ? NULL : &payload, eof ? 0 : 1, &option); } if (status == MMAY_MANAGED_OK) { payload.reference = option; status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, &payload, 1, out); } mmay_owned_bytes_dispose(&line); { MarimayManagedStatus pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; } return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static MMAY_G_UNUSED int mmay_g_io_read_bytes(mmay_g_execution *x, int64_t limit, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedArrayDescriptor *array_descriptor, MarimayManagedReference *out) { MarimayIoInput input = {MMAY_G_STDIN_GRANTED != 0, stdin, mmay_g_stdin_read}; MarimayOwnedBytes bytes = {0}; MarimayManagedCell *cells = NULL, payload = {0}; MarimayManagedReference array = mmay_managed_invalid_reference(); MarimayManagedReference *locations[1] = {&array}; MarimayManagedRootFrame roots = {0}; MarimayManagedStatus status, pop_status; MarimayIoStatus io_status; size_t allocation = 0, index; if (limit < 0 || (uint64_t)limit > MMAY_IO_MAX_BYTES) return mmay_g_io_read_error(x, result_descriptor, error_descriptor, MMAY_IO_INPUT_TOO_LONG, limit, out); io_status = mmay_io_read_bytes(&input, x->allocator, (size_t)limit, &bytes); if (io_status == MMAY_IO_ALLOCATION_FAILED) return 5; if (io_status == MMAY_IO_INVALID_ARGUMENT) return 7; if (io_status != MMAY_IO_OK) return mmay_g_io_read_error(x, result_descriptor, error_descriptor, io_status, limit, out); if (!mmay_size_multiply(bytes.length, sizeof(*cells), &allocation)) { mmay_owned_bytes_dispose(&bytes); return 6; } cells = allocation == 0 ? NULL : mmay_allocate(&x->allocator, allocation); if (allocation != 0 && cells == NULL) { mmay_owned_bytes_dispose(&bytes); return 5; } for (index = 0; index < bytes.length; index += 1) cells[index].f64_bits = bytes.data[index]; status = mmay_managed_root_push(&x->heap, &roots, locations, 1); if (status == MMAY_MANAGED_OK) status = mmay_managed_array_new(&x->heap, array_descriptor, cells, bytes.length, &array); if (status == MMAY_MANAGED_OK) { payload.reference = array; status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; mmay_deallocate(&x->allocator, cells, allocation); mmay_owned_bytes_dispose(&bytes); return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static MMAY_G_UNUSED int mmay_g_io_write_bytes(mmay_g_execution *x, MarimayManagedReference array, int operation, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { static const unsigned char stdout_name[] = \"io.stdout\", stderr_name[] = \"io.stderr\"; bool stderr_stream = operation == 2; const unsigned char *capability_name = stderr_stream ? stderr_name : stdout_name; size_t capability_length = stderr_stream ? sizeof(stderr_name) - 1 : sizeof(stdout_name) - 1; MarimayIoOutput output = {stderr_stream ? MMAY_G_STDERR_GRANTED != 0 : MMAY_G_STDOUT_GRANTED != 0, stderr_stream ? (void *)stderr : (void *)stdout, mmay_g_file_write, mmay_g_file_flush}; unsigned char *bytes = NULL; size_t length = 0; int result; MarimayIoStatus io_status; MarimayManagedStatus status = mmay_managed_enum_new(&x->heap, result_descriptor, 0, NULL, 0, out); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); result = mmay_g_text_array_bytes(x, array, &bytes, &length); if (result != 0) return result; if (!stderr_stream && length != 0 && mmay_test_output_depth(&x->test_output) != 0) { MarimayIoStdout sink = {0}; MarimayTestOutputStatus output_status; sink.capability_granted = MMAY_G_STDOUT_GRANTED != 0; sink.publish = mmay_g_host_stdout; output_status = mmay_test_output_publish(&x->test_output, &sink, bytes, length); mmay_deallocate(&x->allocator, bytes, length); if (output_status == MMAY_TEST_OUTPUT_OK) return 0; if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) return 5; if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) return 6; return mmay_g_io_error(x, result_descriptor, error_descriptor, output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED, capability_name, capability_length, 2, out); } io_status = mmay_io_write(&output, bytes, length); mmay_deallocate(&x->allocator, bytes, length); return io_status == MMAY_IO_OK ? 0 : mmay_g_io_error(x, result_descriptor, error_descriptor, io_status == MMAY_IO_CAPABILITY_DENIED, capability_name, capability_length, 2, out); }\n"
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
    if (instruction->kind == MMAY_MIR_IO_READ_BYTES) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &function->arrays, instruction->array
        );
        if (array == NULL || !mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_io_read_bytes(mmay_g_x, mmay_g_v_%u, &",
                instruction->first
            ) || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, result->type
            ) || !mmay_native_c_write(buffer, ", &")
            || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, error->type
            ) || !mmay_native_c_write(
                buffer,
                ", %s, &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
                mmay_native_c_array_descriptor(function, array->type),
                instruction->target, instruction->span.start,
                instruction->span.end
            )) return false;
        return true;
    }
    if (instruction->kind == MMAY_MIR_IO_WRITE_BYTES) {
        return mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_io_write_bytes(mmay_g_x, mmay_g_v_%u, %d, &",
                instruction->first, (int)instruction->io_operation
            ) && mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, result->type
            ) && mmay_native_c_write(buffer, ", &")
            && mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, error->type
            ) && mmay_native_c_write(
                buffer,
                ", &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
                instruction->target, instruction->span.start,
                instruction->span.end
            );
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
