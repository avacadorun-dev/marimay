#include "vm_internal.h"

#include <string.h>

#include "ruda/runtime/io.h"
#include "ruda/runtime/string.h"

static const RudaBytecodeEnumPayload *ruda_bytecode_run_io_payload(
    const RudaBytecodeFunction *function,
    const RudaBytecodeEnumLayout *layout,
    size_t variant_index,
    size_t field
) {
    const RudaBytecodeEnumVariant *variant = NULL;
    size_t relative = 0;
    if (function == NULL || layout == NULL
        || variant_index >= layout->variant_count
        || layout->variant_start > function->enum_variants.length
        || variant_index >= function->enum_variants.length
            - layout->variant_start) {
        return NULL;
    }
    variant = ruda_vector_at_const(
        &function->enum_variants,
        layout->variant_start + variant_index
    );
    if (variant == NULL || field >= variant->payload_count
        || !ruda_size_add(variant->payload_start, field, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return ruda_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static RudaBytecodeRunStatus ruda_bytecode_run_io_error(
    RudaBytecodeExecution *execution,
    const RudaManagedEnumDescriptor *result_descriptor,
    const RudaManagedEnumDescriptor *error_descriptor,
    RudaIoStdoutStatus io_status,
    RudaBytecodeValue *target
) {
    static const unsigned char capability_name[] = "io.stdout";
    RudaManagedReference capability = ruda_managed_invalid_reference();
    RudaManagedReference error = ruda_managed_invalid_reference();
    RudaManagedReference *locations[2] = {&capability, &error};
    RudaManagedRootFrame roots;
    RudaManagedCell payload;
    RudaManagedStatus managed_status;
    memset(&roots, 0, sizeof(roots));
    memset(&payload, 0, sizeof(payload));
    managed_status = ruda_managed_root_push(
        &execution->managed_heap, &roots, locations, 2
    );
    if (managed_status != RUDA_MANAGED_OK) {
        return ruda_bytecode_run_managed_status(managed_status);
    }
    if (io_status == RUDA_IO_STDOUT_CAPABILITY_DENIED) {
        managed_status = ruda_managed_string_new(
            &execution->managed_heap,
            capability_name,
            sizeof(capability_name) - 1,
            &capability
        );
        if (managed_status == RUDA_MANAGED_OK) {
            payload.reference = capability;
            managed_status = ruda_managed_enum_new(
                &execution->managed_heap, error_descriptor, 0,
                &payload, 1, &error
            );
        }
    } else {
        managed_status = ruda_managed_enum_new(
            &execution->managed_heap, error_descriptor, 1,
            NULL, 0, &error
        );
    }
    if (managed_status == RUDA_MANAGED_OK) {
        payload.reference = error;
        managed_status = ruda_managed_enum_new(
            &execution->managed_heap, result_descriptor, 1,
            &payload, 1, &target->as.managed_reference
        );
    }
    {
        RudaManagedStatus pop_status = ruda_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (managed_status == RUDA_MANAGED_OK) {
            managed_status = pop_status;
        }
    }
    return ruda_bytecode_run_managed_status(managed_status);
}

RudaBytecodeRunStatus ruda_bytecode_run_io_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaAllocator allocator,
    const RudaIoStdout *stdout_sink
) {
    size_t target_index = (size_t)instruction->operands[0].unsigned_value;
    size_t text_index = (size_t)instruction->operands[1].unsigned_value;
    uint32_t result_id = ruda_bytecode_register_layout(
        function, (uint32_t)target_index
    );
    const RudaBytecodeEnumLayout *result_layout = ruda_bytecode_enum_layout(
        function, result_id
    );
    const RudaBytecodeEnumPayload *error_payload =
        ruda_bytecode_run_io_payload(function, result_layout, 1, 0);
    const void *result_descriptor_raw = NULL;
    const void *error_descriptor_raw = NULL;
    const unsigned char *text = NULL;
    size_t text_length = 0;
    size_t line_length = 0;
    unsigned char *line = NULL;
    RudaBytecodeRunStatus status;
    RudaIoStdoutStatus io_status;
    if (execution == NULL || !execution->initialized || stdout_sink == NULL
        || result_layout == NULL || error_payload == NULL
        || error_payload->type != RUDA_BYTECODE_TYPE_ENUM) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    status = ruda_bytecode_run_nominal_descriptor(
        execution, function, RUDA_BYTECODE_TYPE_ENUM,
        result_id, &result_descriptor_raw
    );
    if (status == RUDA_BYTECODE_RUN_OK) {
        status = ruda_bytecode_run_nominal_descriptor(
            execution, function, RUDA_BYTECODE_TYPE_ENUM,
            error_payload->layout, &error_descriptor_raw
        );
    }
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    status = ruda_bytecode_run_managed_status(ruda_managed_enum_new(
        &execution->managed_heap,
        result_descriptor_raw,
        0,
        NULL,
        0,
        &registers[target_index].as.managed_reference
    ));
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    if (!stdout_sink->capability_granted) {
        return ruda_bytecode_run_io_error(
            execution, result_descriptor_raw, error_descriptor_raw,
            RUDA_IO_STDOUT_CAPABILITY_DENIED, &registers[target_index]
        );
    }
    if (!ruda_bytecode_execution_string_bytes(
            execution, registers[text_index], &text, &text_length
        )) {
        return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (!ruda_size_add(text_length, 1, &line_length)) {
        return RUDA_BYTECODE_RUN_LIMIT_REACHED;
    }
    line = ruda_allocate(&allocator, line_length);
    if (line == NULL) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (text_length != 0) {
        memcpy(line, text, text_length);
    }
    line[text_length] = '\n';
    io_status = ruda_io_stdout_publish_line(stdout_sink, line, line_length);
    ruda_deallocate(&allocator, line, line_length);
    if (io_status == RUDA_IO_STDOUT_OK) {
        return RUDA_BYTECODE_RUN_OK;
    }
    if (io_status != RUDA_IO_STDOUT_WRITE_FAILED) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    return ruda_bytecode_run_io_error(
        execution, result_descriptor_raw, error_descriptor_raw,
        io_status, &registers[target_index]
    );
}
