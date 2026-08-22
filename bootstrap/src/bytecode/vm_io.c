#include "vm_internal.h"

#include <string.h>

#include "mmay/runtime/io.h"
#include "mmay/runtime/string.h"

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

static MarimayBytecodeRunStatus mmay_bytecode_run_io_error(
    MarimayBytecodeExecution *execution,
    const MarimayManagedEnumDescriptor *result_descriptor,
    const MarimayManagedEnumDescriptor *error_descriptor,
    MarimayIoStdoutStatus io_status,
    MarimayBytecodeValue *target
) {
    static const unsigned char capability_name[] = "io.stdout";
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
    if (io_status == MMAY_IO_STDOUT_CAPABILITY_DENIED) {
        managed_status = mmay_managed_string_new(
            &execution->managed_heap,
            capability_name,
            sizeof(capability_name) - 1,
            &capability
        );
        if (managed_status == MMAY_MANAGED_OK) {
            payload.reference = capability;
            managed_status = mmay_managed_enum_new(
                &execution->managed_heap, error_descriptor, 0,
                &payload, 1, &error
            );
        }
    } else {
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, error_descriptor, 1,
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
    const void *result_descriptor_raw = NULL;
    const void *error_descriptor_raw = NULL;
    const unsigned char *text = NULL;
    size_t text_length = 0;
    size_t line_length = 0;
    unsigned char *line = NULL;
    MarimayBytecodeRunStatus status;
    MarimayTestOutputStatus output_status;
    if (execution == NULL || !execution->initialized || stdout_sink == NULL
        || result_layout == NULL || error_payload == NULL
        || error_payload->type != MMAY_BYTECODE_TYPE_ENUM) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
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
    if (!mmay_bytecode_execution_string_bytes(
            execution, registers[text_index], &text, &text_length
        )) {
        return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (!mmay_size_add(text_length, 1, &line_length)) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    line = mmay_allocate(&allocator, line_length);
    if (line == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (text_length != 0) {
        memcpy(line, text, text_length);
    }
    line[text_length] = '\n';
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
        output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED
            ? MMAY_IO_STDOUT_CAPABILITY_DENIED
            : MMAY_IO_STDOUT_WRITE_FAILED,
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
