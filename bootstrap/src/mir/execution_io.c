#include "mir_internal.h"

#include <stdint.h>
#include <string.h>

#include "mmay/runtime/io.h"

static const MarimayMirEnum *mmay_mir_execute_io_enum_by_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (layout != NULL && layout->type == type) {
            return layout;
        }
    }
    return NULL;
}

static MarimayMirInterpretStatus mmay_mir_execute_io_error(
    const MarimayMirFunction *function,
    const MarimayMirEnum *result_layout,
    const MarimayMirEnum *error_layout,
    MarimayIoStatus io_status,
    const unsigned char *capability_name,
    size_t capability_length,
    int64_t limit,
    MarimayMirExecution *execution,
    MarimayMirValue *target
) {
    MarimayMirValue error_value;
    MarimayMirValue error_payload;
    MarimayMirInterpretStatus status;
    size_t variant = io_status == MMAY_IO_CAPABILITY_DENIED ? 0
        : io_status == MMAY_IO_READ_FAILED ? 1
        : io_status == MMAY_IO_WRITE_FAILED ? 2
        : io_status == MMAY_IO_FLUSH_FAILED ? 3
        : io_status == MMAY_IO_INVALID_UTF8 ? 4 : 5;
    memset(&error_value, 0, sizeof(error_value));
    memset(&error_payload, 0, sizeof(error_payload));
    error_value.type = error_layout->type;
    if (io_status == MMAY_IO_CAPABILITY_DENIED) {
        error_payload.type = MMAY_TYPE_STRING;
        status = mmay_mir_execution_new_string(
            execution,
            capability_name,
            capability_length,
            &error_payload.as.reference
        );
        if (status != MMAY_MIR_INTERPRET_OK) {
            return status;
        }
        status = mmay_mir_execution_new_enum(
            execution, function, error_layout, 0,
            &error_payload, 1, &error_value.as.reference
        );
    } else if (io_status == MMAY_IO_LINE_TOO_LONG) {
        error_payload.type = MMAY_TYPE_I64;
        error_payload.as.i64 = limit;
        status = mmay_mir_execution_new_enum(
            execution, function, error_layout, variant,
            &error_payload, 1, &error_value.as.reference
        );
    } else {
        status = mmay_mir_execution_new_enum(
            execution, function, error_layout,
            error_layout->variant_count == 2 ? 1 : variant,
            NULL, 0, &error_value.as.reference
        );
    }
    if (status != MMAY_MIR_INTERPRET_OK) {
        return status;
    }
    return mmay_mir_execution_new_enum(
        execution, function, result_layout, 1,
        &error_value, 1, &target->as.reference
    );
}

MarimayMirInterpretStatus mmay_mir_execute_io_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    static const unsigned char stdin_name[] = "io.stdin";
    static const unsigned char stdout_name[] = "io.stdout";
    static const unsigned char stderr_name[] = "io.stderr";
    const MarimayMirEnum *result_layout = mmay_mir_enum(
        function, instruction->enum_id
    );
    const MarimayMirEnumVariant *error_variant = mmay_mir_enum_variant(
        function, result_layout, 1
    );
    const MarimayMirEnumPayload *error_payload = mmay_mir_enum_payload(
        function, error_variant, 0
    );
    const MarimayMirEnum *error_layout = error_payload != NULL
        ? mmay_mir_execute_io_enum_by_type(function, error_payload->type)
        : NULL;
    const MarimayMirManagedString *text = NULL;
    const MarimayMirEnum *option_layout = NULL;
    unsigned char *line = NULL;
    size_t line_length = 0;
    MarimayMirInterpretStatus status;
    MarimayTestOutputStatus output_status;
    MarimayIoStatus io_status = MMAY_IO_OK;
    MarimayIoOutput output;
    bool stderr_stream = instruction->io_operation >= MMAY_MIR_IO_STDERR_WRITE
        && instruction->io_operation <= MMAY_MIR_IO_STDERR_LINE;
    bool newline = instruction->kind == MMAY_MIR_IO_PRINTLN
        || instruction->io_operation == MMAY_MIR_IO_STDOUT_LINE
        || instruction->io_operation == MMAY_MIR_IO_STDERR_LINE;
    const unsigned char *capability_name = stderr_stream
        ? stderr_name : stdout_name;
    size_t capability_length = stderr_stream
        ? sizeof(stderr_name) - 1 : sizeof(stdout_name) - 1;
    if (result_layout == NULL || error_layout == NULL
        || options.stdout_sink == NULL || options.execution == NULL) {
        return options.stdout_sink == NULL || options.execution == NULL
            ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
            : MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (instruction->kind == MMAY_MIR_IO_READ_LINE) {
        MarimayIoInput input = {
            options.stdout_sink->stdin_capability_granted,
            options.stdout_sink->stdin_context,
            options.stdout_sink->read_stdin
        };
        MarimayOwnedBytes bytes = {0};
        MarimayMirValue string_value = {0};
        MarimayMirValue option_value = {0};
        bool eof = false;
        int64_t limit = source != NULL ? source->as.i64 : -1;
        option_layout = mmay_mir_enum(function, instruction->option_enum_id);
        if (source == NULL || option_layout == NULL) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        if (limit < 0 || (uint64_t)limit > MMAY_IO_MAX_LINE_BYTES) {
            return mmay_mir_execute_io_error(
                function, result_layout, error_layout,
                MMAY_IO_LINE_TOO_LONG, NULL, 0,
                limit,
                options.execution, target
            );
        }
        io_status = mmay_io_read_line(
            &input, *allocator, (size_t)limit, &bytes, &eof
        );
        if (io_status != MMAY_IO_OK) {
            mmay_owned_bytes_dispose(&bytes);
            if (io_status == MMAY_IO_ALLOCATION_FAILED) {
                return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            if (io_status == MMAY_IO_INVALID_ARGUMENT) {
                return MMAY_MIR_INTERPRET_INVALID_MIR;
            }
            return mmay_mir_execute_io_error(
                function, result_layout, error_layout, io_status,
                stdin_name, sizeof(stdin_name) - 1, limit,
                options.execution, target
            );
        }
        string_value.type = MMAY_TYPE_STRING;
        option_value.type = option_layout->type;
        status = eof ? MMAY_MIR_INTERPRET_OK
            : mmay_mir_execution_new_string(
                options.execution, bytes.data, bytes.length,
                &string_value.as.reference
            );
        if (status == MMAY_MIR_INTERPRET_OK) {
            status = mmay_mir_execution_new_enum(
                options.execution, function, option_layout, eof ? 1 : 0,
                eof ? NULL : &string_value, eof ? 0 : 1,
                &option_value.as.reference
            );
        }
        if (status == MMAY_MIR_INTERPRET_OK) {
            status = mmay_mir_execution_new_enum(
                options.execution, function, result_layout, 0,
                &option_value, 1, &target->as.reference
            );
        }
        mmay_owned_bytes_dispose(&bytes);
        return status;
    }
    status = mmay_mir_execution_new_enum(
        options.execution, function, result_layout, 0,
        NULL, 0, &target->as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) {
        return status;
    }
    if (instruction->kind == MMAY_MIR_IO_FLUSH) {
        stderr_stream = instruction->io_operation == MMAY_MIR_IO_STDERR_FLUSH;
        capability_name = stderr_stream ? stderr_name : stdout_name;
        capability_length = stderr_stream
            ? sizeof(stderr_name) - 1 : sizeof(stdout_name) - 1;
        output.capability_granted = stderr_stream
            ? options.stdout_sink->stderr_capability_granted
            : options.stdout_sink->capability_granted;
        output.context = stderr_stream
            ? options.stdout_sink->stderr_context
            : options.stdout_sink->context;
        output.write = stderr_stream
            ? options.stdout_sink->write_stderr
            : options.stdout_sink->write_stdout;
        output.flush = stderr_stream
            ? options.stdout_sink->flush_stderr
            : options.stdout_sink->flush_stdout;
        io_status = mmay_io_flush(&output);
        if (io_status == MMAY_IO_OK) return MMAY_MIR_INTERPRET_OK;
        return mmay_mir_execute_io_error(
            function, result_layout, error_layout, io_status,
            capability_name, capability_length, 0,
            options.execution, target
        );
    }
    text = source != NULL
        ? mmay_mir_execution_string_value(options.execution, *source) : NULL;
    if (text == NULL || (newline
            && !mmay_size_add(text->length, 1, &line_length))) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (!newline) line_length = text->length;
    if (newline) {
        line = mmay_allocate(allocator, line_length);
        if (line == NULL) return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        if (text->length != 0) memcpy(line, text->bytes, text->length);
        line[text->length] = '\n';
    }
    if (instruction->kind == MMAY_MIR_IO_PRINTLN
        || (line_length != 0 && !stderr_stream
            && mmay_test_output_depth(&options.execution->test_output) != 0)) {
        output_status = mmay_test_output_publish(
            &options.execution->test_output, options.stdout_sink,
            newline ? line : text->bytes, line_length
        );
        if (line != NULL) mmay_deallocate(allocator, line, line_length);
        if (output_status == MMAY_TEST_OUTPUT_OK) return MMAY_MIR_INTERPRET_OK;
        if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) {
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) {
            return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        }
        io_status = output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED
            ? MMAY_IO_CAPABILITY_DENIED : MMAY_IO_WRITE_FAILED;
    } else {
        output.capability_granted = stderr_stream
            ? options.stdout_sink->stderr_capability_granted
            : options.stdout_sink->capability_granted;
        output.context = stderr_stream
            ? options.stdout_sink->stderr_context
            : options.stdout_sink->context;
        output.write = stderr_stream
            ? options.stdout_sink->write_stderr
            : options.stdout_sink->write_stdout;
        output.flush = stderr_stream
            ? options.stdout_sink->flush_stderr
            : options.stdout_sink->flush_stdout;
        io_status = mmay_io_write(
            &output, newline ? line : text->bytes, line_length
        );
        if (line != NULL) mmay_deallocate(allocator, line, line_length);
        if (io_status == MMAY_IO_OK) return MMAY_MIR_INTERPRET_OK;
    }
    return mmay_mir_execute_io_error(
        function, result_layout, error_layout, io_status,
        capability_name, capability_length, 0, options.execution, target
    );
}

MarimayMirInterpretStatus mmay_mir_execute_test_output_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    const MarimayMirManagedString *expected = NULL;
    const MarimayOwnedBytes *failure = NULL;
    MarimayTestOutputStatus output_status;
    if (function == NULL || instruction == NULL || source == NULL
        || target == NULL || allocator == NULL || options.execution == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (instruction->kind == MMAY_MIR_TEST_OUTPUT_BEGIN) {
        expected = mmay_mir_execution_string_value(
            options.execution, *source
        );
        if (expected == NULL) return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
        output_status = mmay_test_output_begin(
            &options.execution->test_output, *allocator,
            expected->bytes, expected->length
        );
        if (output_status == MMAY_TEST_OUTPUT_OK) {
            target->as.boolean = true;
            return MMAY_MIR_INTERPRET_OK;
        }
    } else {
        if (!source->as.boolean) return MMAY_MIR_INTERPRET_INVALID_MIR;
        output_status = mmay_test_output_end(
            &options.execution->test_output
        );
        if (output_status == MMAY_TEST_OUTPUT_OK) {
            return MMAY_MIR_INTERPRET_OK;
        }
        if (output_status == MMAY_TEST_OUTPUT_MISMATCH) {
            if (options.panic == NULL) return MMAY_MIR_INTERPRET_PANIC;
            failure = mmay_test_output_failure(
                &options.execution->test_output
            );
            if (failure == NULL || !mmay_owned_bytes_copy(
                    &options.panic->message, *allocator,
                    failure->data, failure->length
                )) {
                return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            options.panic->code = "G4004";
            options.panic->symbol = function->symbol;
            options.panic->span = instruction->span;
            return MMAY_MIR_INTERPRET_PANIC;
        }
    }
    if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    return MMAY_MIR_INTERPRET_INVALID_MIR;
}
