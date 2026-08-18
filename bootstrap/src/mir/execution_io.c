#include "mir_internal.h"

#include <string.h>

#include "ruda/runtime/io.h"

static const RudaMirEnum *ruda_mir_execute_io_enum_by_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const RudaMirEnum *layout = ruda_mir_enum(
            function, (RudaMirEnumId)index
        );
        if (layout != NULL && layout->type == type) {
            return layout;
        }
    }
    return NULL;
}

static RudaMirInterpretStatus ruda_mir_execute_io_error(
    const RudaMirFunction *function,
    const RudaMirEnum *result_layout,
    const RudaMirEnum *error_layout,
    RudaIoStdoutStatus io_status,
    RudaMirExecution *execution,
    RudaMirValue *target
) {
    static const unsigned char capability_name[] = "io.stdout";
    RudaMirValue error_value;
    RudaMirValue error_payload;
    RudaMirInterpretStatus status;
    memset(&error_value, 0, sizeof(error_value));
    memset(&error_payload, 0, sizeof(error_payload));
    error_value.type = error_layout->type;
    if (io_status == RUDA_IO_STDOUT_CAPABILITY_DENIED) {
        error_payload.type = RUDA_TYPE_STRING;
        status = ruda_mir_execution_new_string(
            execution,
            capability_name,
            sizeof(capability_name) - 1,
            &error_payload.as.reference
        );
        if (status != RUDA_MIR_INTERPRET_OK) {
            return status;
        }
        status = ruda_mir_execution_new_enum(
            execution, function, error_layout, 0,
            &error_payload, 1, &error_value.as.reference
        );
    } else {
        status = ruda_mir_execution_new_enum(
            execution, function, error_layout, 1,
            NULL, 0, &error_value.as.reference
        );
    }
    if (status != RUDA_MIR_INTERPRET_OK) {
        return status;
    }
    return ruda_mir_execution_new_enum(
        execution, function, result_layout, 1,
        &error_value, 1, &target->as.reference
    );
}

RudaMirInterpretStatus ruda_mir_execute_io_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaMirValue *source,
    RudaMirValue *target,
    const RudaAllocator *allocator,
    RudaMirInterpretOptions options
) {
    const RudaMirEnum *result_layout = ruda_mir_enum(
        function, instruction->enum_id
    );
    const RudaMirEnumVariant *error_variant = ruda_mir_enum_variant(
        function, result_layout, 1
    );
    const RudaMirEnumPayload *error_payload = ruda_mir_enum_payload(
        function, error_variant, 0
    );
    const RudaMirEnum *error_layout = error_payload != NULL
        ? ruda_mir_execute_io_enum_by_type(function, error_payload->type)
        : NULL;
    const RudaMirManagedString *text = options.execution != NULL
        ? ruda_mir_execution_string_value(options.execution, *source) : NULL;
    unsigned char *line = NULL;
    size_t line_length = 0;
    RudaMirInterpretStatus status;
    RudaIoStdoutStatus io_status;
    if (result_layout == NULL || error_layout == NULL || text == NULL
        || options.stdout_sink == NULL || options.execution == NULL
        || !ruda_size_add(text->length, 1, &line_length)) {
        return options.stdout_sink == NULL || options.execution == NULL
            ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
            : RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    status = ruda_mir_execution_new_enum(
        options.execution, function, result_layout, 0,
        NULL, 0, &target->as.reference
    );
    if (status != RUDA_MIR_INTERPRET_OK) {
        return status;
    }
    if (!options.stdout_sink->capability_granted) {
        return ruda_mir_execute_io_error(
            function, result_layout, error_layout,
            RUDA_IO_STDOUT_CAPABILITY_DENIED, options.execution, target
        );
    }
    line = ruda_allocate(allocator, line_length);
    if (line == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (text->length != 0) {
        memcpy(line, text->bytes, text->length);
    }
    line[text->length] = '\n';
    io_status = ruda_io_stdout_publish_line(
        options.stdout_sink, line, line_length
    );
    ruda_deallocate(allocator, line, line_length);
    if (io_status == RUDA_IO_STDOUT_OK) {
        return RUDA_MIR_INTERPRET_OK;
    }
    if (io_status != RUDA_IO_STDOUT_CAPABILITY_DENIED
        && io_status != RUDA_IO_STDOUT_WRITE_FAILED) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    return ruda_mir_execute_io_error(
        function, result_layout, error_layout, io_status,
        options.execution, target
    );
}
