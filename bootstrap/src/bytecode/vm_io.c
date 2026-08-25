#include "vm_internal.h"

#include <string.h>

#include "mmay/runtime/io.h"
#include "mmay/runtime/string.h"

static MarimayBytecodeRunStatus mmay_bytecode_run_io_error(
    MarimayBytecodeExecution *execution,
    const MarimayManagedEnumDescriptor *result_descriptor,
    const MarimayManagedEnumDescriptor *error_descriptor,
    bool denied, const unsigned char *capability_name, size_t capability_length,
    size_t error_variant, int64_t error_limit, MarimayBytecodeValue *target
);

static const MarimayBytecodeEnumPayload *mmay_bytecode_run_io_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant_index,
    size_t field
) {
    const MarimayBytecodeEnumVariant *variant = NULL;
    size_t relative = 0;
    if (function == NULL || layout == NULL
        || variant_index >= layout->variant_count
        || layout->variant_start > function->enum_variants.length
        || variant_index >= function->enum_variants.length
            - layout->variant_start) {
        return NULL;
    }
    variant = mmay_vector_at_const(
        &function->enum_variants,
        layout->variant_start + variant_index
    );
    if (variant == NULL || field >= variant->payload_count
        || !mmay_size_add(variant->payload_start, field, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static MarimayBytecodeRunStatus mmay_bytecode_run_io_read_line(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    const MarimayIoStdout *stdout_sink
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    size_t limit_register = (size_t)instruction->operands[1].unsigned_value;
    uint32_t result_id = mmay_bytecode_register_layout(function, (uint32_t)target);
    uint32_t option_id = (uint32_t)instruction->operands[2].signed_value;
    const MarimayBytecodeEnumLayout *result_layout = mmay_bytecode_enum_layout(function, result_id);
    const MarimayBytecodeEnumPayload *error_payload = mmay_bytecode_run_io_payload(function, result_layout, 1, 0);
    const void *result_descriptor = NULL;
    const void *error_descriptor = NULL;
    const void *option_descriptor = NULL;
    MarimayIoInput input = {
        stdout_sink != NULL && stdout_sink->stdin_capability_granted,
        stdout_sink != NULL ? stdout_sink->stdin_context : NULL,
        stdout_sink != NULL ? stdout_sink->read_stdin : NULL
    };
    MarimayOwnedBytes line = {0};
    MarimayManagedReference string = mmay_managed_invalid_reference();
    MarimayManagedReference option = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[2] = {&string, &option};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell payload = {0};
    MarimayManagedStatus managed_status;
    MarimayBytecodeRunStatus status;
    MarimayIoStatus io_status;
    bool eof = false;
    int64_t limit = 0;
    if (execution == NULL || function == NULL || result_layout == NULL
        || error_payload == NULL || error_payload->type != MMAY_BYTECODE_TYPE_ENUM
        || instruction->operands[2].signed_value < 0) return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    status = mmay_bytecode_run_nominal_descriptor(execution, function, MMAY_BYTECODE_TYPE_ENUM, result_id, &result_descriptor);
    if (status == MMAY_BYTECODE_RUN_OK) status = mmay_bytecode_run_nominal_descriptor(execution, function, MMAY_BYTECODE_TYPE_ENUM, error_payload->layout, &error_descriptor);
    if (status == MMAY_BYTECODE_RUN_OK) status = mmay_bytecode_run_nominal_descriptor(execution, function, MMAY_BYTECODE_TYPE_ENUM, option_id, &option_descriptor);
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    limit = registers[limit_register].as.i64;
    if (limit < 0 || (uint64_t)limit > MMAY_IO_MAX_LINE_BYTES) {
        return mmay_bytecode_run_io_error(execution, result_descriptor, error_descriptor,
            false, NULL, 0, 5, limit, &registers[target]);
    }
    io_status = mmay_io_read_line(&input, execution->allocator, (size_t)limit, &line, &eof);
    if (io_status != MMAY_IO_OK) {
        if (io_status == MMAY_IO_ALLOCATION_FAILED) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        if (io_status == MMAY_IO_INVALID_ARGUMENT) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        size_t variant = io_status == MMAY_IO_CAPABILITY_DENIED ? 0 : io_status == MMAY_IO_READ_FAILED ? 1 : io_status == MMAY_IO_INVALID_UTF8 ? 4 : io_status == MMAY_IO_LINE_TOO_LONG ? 5 : 1;
        return mmay_bytecode_run_io_error(execution, result_descriptor, error_descriptor, variant == 0, (const unsigned char *)"io.stdin", 8, variant, limit, &registers[target]);
    }
    managed_status = mmay_managed_root_push(&execution->managed_heap, &roots, locations, 2);
    if (managed_status == MMAY_MANAGED_OK && !eof) managed_status = mmay_managed_string_new(&execution->managed_heap, line.data, line.length, &string);
    if (managed_status == MMAY_MANAGED_OK) {
        payload.reference = string;
        managed_status = mmay_managed_enum_new(&execution->managed_heap, option_descriptor, eof ? 1 : 0, eof ? NULL : &payload, eof ? 0 : 1, &option);
    }
    if (managed_status == MMAY_MANAGED_OK) {
        payload.reference = option;
        managed_status = mmay_managed_enum_new(&execution->managed_heap, result_descriptor, 0, &payload, 1, &registers[target].as.managed_reference);
    }
    mmay_owned_bytes_dispose(&line);
    {
        MarimayManagedStatus pop_status = mmay_managed_root_pop(&execution->managed_heap, &roots);
        if (managed_status == MMAY_MANAGED_OK) managed_status = pop_status;
    }
    return mmay_bytecode_run_managed_status(managed_status);
}

static MarimayBytecodeRunStatus mmay_bytecode_run_io_error(
    MarimayBytecodeExecution *execution,
    const MarimayManagedEnumDescriptor *result_descriptor,
    const MarimayManagedEnumDescriptor *error_descriptor,
    bool denied,
    const unsigned char *capability_name,
    size_t capability_length,
    size_t error_variant,
    int64_t error_limit,
    MarimayBytecodeValue *target
) {
    MarimayManagedReference capability = mmay_managed_invalid_reference();
    MarimayManagedReference error = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[2] = {&capability, &error};
    MarimayManagedRootFrame roots;
    MarimayManagedCell payload;
    MarimayManagedStatus managed_status;
    memset(&roots, 0, sizeof(roots));
    memset(&payload, 0, sizeof(payload));
    managed_status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 2
    );
    if (managed_status != MMAY_MANAGED_OK) {
        return mmay_bytecode_run_managed_status(managed_status);
    }
    if (denied) {
        managed_status = mmay_managed_string_new(
            &execution->managed_heap,
            capability_name,
            capability_length,
            &capability
        );
        if (managed_status == MMAY_MANAGED_OK) {
            payload.reference = capability;
            managed_status = mmay_managed_enum_new(
                &execution->managed_heap, error_descriptor, 0,
                &payload, 1, &error
            );
        }
    } else if (error_variant == 5) {
        payload.f64_bits = (uint64_t)error_limit;
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, error_descriptor, error_variant,
            &payload, 1, &error
        );
    } else {
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, error_descriptor, error_variant,
            NULL, 0, &error
        );
    }
    if (managed_status == MMAY_MANAGED_OK) {
        payload.reference = error;
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, result_descriptor, 1,
            &payload, 1, &target->as.managed_reference
        );
    }
    {
        MarimayManagedStatus pop_status = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (managed_status == MMAY_MANAGED_OK) {
            managed_status = pop_status;
        }
    }
    return mmay_bytecode_run_managed_status(managed_status);
}

MarimayBytecodeRunStatus mmay_bytecode_run_io_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const MarimayIoStdout *stdout_sink
) {
    size_t target_index = (size_t)instruction->operands[0].unsigned_value;
    size_t text_index = (size_t)instruction->operands[1].unsigned_value;
    uint32_t result_id = mmay_bytecode_register_layout(
        function, (uint32_t)target_index
    );
    const MarimayBytecodeEnumLayout *result_layout = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumPayload *error_payload =
        mmay_bytecode_run_io_payload(function, result_layout, 1, 0);
    const MarimayBytecodeEnumLayout *io_error_layout = error_payload != NULL
        ? mmay_bytecode_enum_layout(function, error_payload->layout) : NULL;
    const void *result_descriptor_raw = NULL;
    const void *error_descriptor_raw = NULL;
    const unsigned char *text = NULL;
    size_t text_length = 0;
    size_t line_length = 0;
    unsigned char *line = NULL;
    MarimayBytecodeRunStatus status;
    MarimayTestOutputStatus output_status;
    bool cp32 = instruction->opcode == MMAY_OPCODE_IO_WRITE
        || instruction->opcode == MMAY_OPCODE_IO_FLUSH;
    bool stderr_stream = false;
    bool newline = true;
    MarimayIoStatus io_status;
    MarimayIoOutput output;
    static const unsigned char stdout_name[] = "io.stdout";
    static const unsigned char stderr_name[] = "io.stderr";
    if (execution == NULL || !execution->initialized || stdout_sink == NULL
        || result_layout == NULL || error_payload == NULL
        || error_payload->type != MMAY_BYTECODE_TYPE_ENUM) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_READ_LINE) {
        return mmay_bytecode_run_io_read_line(
            instruction, registers, execution, function, stdout_sink
        );
    }
    status = mmay_bytecode_run_nominal_descriptor(
        execution, function, MMAY_BYTECODE_TYPE_ENUM,
        result_id, &result_descriptor_raw
    );
    if (status == MMAY_BYTECODE_RUN_OK) {
        status = mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            error_payload->layout, &error_descriptor_raw
        );
    }
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    status = mmay_bytecode_run_managed_status(mmay_managed_enum_new(
        &execution->managed_heap,
        result_descriptor_raw,
        0,
        NULL,
        0,
        &registers[target_index].as.managed_reference
    ));
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_FLUSH) {
        stderr_stream = instruction->operands[1].signed_value == 5;
        output.capability_granted = stderr_stream
            ? stdout_sink->stderr_capability_granted
            : stdout_sink->capability_granted;
        output.context = stderr_stream
            ? stdout_sink->stderr_context : stdout_sink->context;
        output.write = stderr_stream
            ? stdout_sink->write_stderr : stdout_sink->write_stdout;
        output.flush = stderr_stream
            ? stdout_sink->flush_stderr : stdout_sink->flush_stdout;
        io_status = mmay_io_flush(&output);
        if (io_status == MMAY_IO_OK) {
            return MMAY_BYTECODE_RUN_OK;
        }
        return mmay_bytecode_run_io_error(
            execution, result_descriptor_raw, error_descriptor_raw,
            io_status == MMAY_IO_CAPABILITY_DENIED,
            stderr_stream ? stderr_name : stdout_name,
            stderr_stream ? sizeof(stderr_name) - 1 : sizeof(stdout_name) - 1,
            3, 0, &registers[target_index]
        );
    }
    if (!mmay_bytecode_execution_string_bytes(
            execution, registers[text_index], &text, &text_length
        )) {
        return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_WRITE) {
        int64_t operation = instruction->operands[2].signed_value;
        stderr_stream = operation >= 2;
        newline = operation == 1 || operation == 3;
    }
    if (newline && !mmay_size_add(text_length, 1, &line_length)) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    if (!newline) line_length = text_length;
    line = line_length == 0 ? NULL : mmay_allocate(&allocator, line_length);
    if (line_length != 0 && line == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (text_length != 0) {
        memcpy(line, text, text_length);
    }
    if (newline) line[text_length] = '\n';
    if (cp32) {
        if (line_length != 0 && !stderr_stream
            && mmay_test_output_depth(&execution->test_output) != 0) {
            output_status = mmay_test_output_publish(
                &execution->test_output, stdout_sink, line, line_length
            );
            if (line != NULL) mmay_deallocate(&allocator, line, line_length);
            if (output_status == MMAY_TEST_OUTPUT_OK) {
                return MMAY_BYTECODE_RUN_OK;
            }
            if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) {
                return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) {
                return MMAY_BYTECODE_RUN_LIMIT_REACHED;
            }
            return mmay_bytecode_run_io_error(
                execution, result_descriptor_raw, error_descriptor_raw,
                output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED,
                stdout_name, sizeof(stdout_name) - 1, 2, 0,
                &registers[target_index]
            );
        }
        output.capability_granted = stderr_stream
            ? stdout_sink->stderr_capability_granted
            : stdout_sink->capability_granted;
        output.context = stderr_stream
            ? stdout_sink->stderr_context : stdout_sink->context;
        output.write = stderr_stream
            ? stdout_sink->write_stderr : stdout_sink->write_stdout;
        output.flush = stderr_stream
            ? stdout_sink->flush_stderr : stdout_sink->flush_stdout;
        io_status = mmay_io_write(&output, line, line_length);
        if (io_status == MMAY_IO_OK) {
            if (line != NULL) mmay_deallocate(&allocator, line, line_length);
            return MMAY_BYTECODE_RUN_OK;
        }
        if (line != NULL) mmay_deallocate(&allocator, line, line_length);
        return mmay_bytecode_run_io_error(
            execution, result_descriptor_raw, error_descriptor_raw,
            io_status == MMAY_IO_CAPABILITY_DENIED,
            stderr_stream ? stderr_name : stdout_name,
            stderr_stream ? sizeof(stderr_name) - 1 : sizeof(stdout_name) - 1,
            2, 0, &registers[target_index]
        );
    }
    output_status = mmay_test_output_publish(
        &execution->test_output, stdout_sink, line, line_length
    );
    mmay_deallocate(&allocator, line, line_length);
    if (output_status == MMAY_TEST_OUTPUT_OK) {
        return MMAY_BYTECODE_RUN_OK;
    }
    if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    if (output_status != MMAY_TEST_OUTPUT_CAPABILITY_DENIED
        && output_status != MMAY_TEST_OUTPUT_WRITE_FAILED) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return mmay_bytecode_run_io_error(
        execution, result_descriptor_raw, error_descriptor_raw,
        output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED,
        stdout_name, sizeof(stdout_name) - 1,
        cp32 || io_error_layout == NULL || io_error_layout->variant_count != 2
            ? 2 : 1, 0,
        &registers[target_index]
    );
}

MarimayBytecodeRunStatus mmay_bytecode_run_test_output_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodePanic *panic,
    MarimaySymbolId symbol
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    size_t source = (size_t)instruction->operands[1].unsigned_value;
    const unsigned char *expected = NULL;
    const MarimayOwnedBytes *failure = NULL;
    size_t expected_length = 0;
    MarimayTestOutputStatus output_status;
    if (execution == NULL || !execution->initialized || function == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (instruction->opcode == MMAY_OPCODE_TEST_OUTPUT_BEGIN) {
        if (!mmay_bytecode_execution_string_bytes(
                execution, registers[source], &expected, &expected_length
            )) {
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        }
        output_status = mmay_test_output_begin(
            &execution->test_output, allocator, expected, expected_length
        );
        if (output_status == MMAY_TEST_OUTPUT_OK) {
            registers[target].as.boolean = true;
            return MMAY_BYTECODE_RUN_OK;
        }
    } else {
        if (!registers[source].as.boolean) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        output_status = mmay_test_output_end(&execution->test_output);
        if (output_status == MMAY_TEST_OUTPUT_OK) {
            return MMAY_BYTECODE_RUN_OK;
        }
        if (output_status == MMAY_TEST_OUTPUT_MISMATCH) {
            failure = mmay_test_output_failure(&execution->test_output);
            return failure == NULL
                ? MMAY_BYTECODE_RUN_INVALID_BYTECODE
                : mmay_bytecode_run_begin_test_panic(
                    panic, allocator, failure->data, failure->length,
                    symbol, instruction->span
                );
        }
    }
    if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}
