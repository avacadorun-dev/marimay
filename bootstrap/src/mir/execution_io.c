#include "mir_internal.h"

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
    MarimayIoStdoutStatus io_status,
    MarimayMirExecution *execution,
    MarimayMirValue *target
) {
    static const unsigned char capability_name[] = "io.stdout";
    MarimayMirValue error_value;
    MarimayMirValue error_payload;
    MarimayMirInterpretStatus status;
    memset(&error_value, 0, sizeof(error_value));
    memset(&error_payload, 0, sizeof(error_payload));
    error_value.type = error_layout->type;
    if (io_status == MMAY_IO_STDOUT_CAPABILITY_DENIED) {
        error_payload.type = MMAY_TYPE_STRING;
        status = mmay_mir_execution_new_string(
            execution,
            capability_name,
            sizeof(capability_name) - 1,
            &error_payload.as.reference
        );
        if (status != MMAY_MIR_INTERPRET_OK) {
            return status;
        }
        status = mmay_mir_execution_new_enum(
            execution, function, error_layout, 0,
            &error_payload, 1, &error_value.as.reference
        );
    } else {
        status = mmay_mir_execution_new_enum(
            execution, function, error_layout,
            error_layout->variant_count == 2 ? 1 : 2,
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
    const MarimayMirManagedString *text = options.execution != NULL
        ? mmay_mir_execution_string_value(options.execution, *source) : NULL;
    unsigned char *line = NULL;
    size_t line_length = 0;
    MarimayMirInterpretStatus status;
    MarimayTestOutputStatus output_status;
    if (result_layout == NULL || error_layout == NULL || text == NULL
        || options.stdout_sink == NULL || options.execution == NULL
        || !mmay_size_add(text->length, 1, &line_length)) {
        return options.stdout_sink == NULL || options.execution == NULL
            ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
            : MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    status = mmay_mir_execution_new_enum(
        options.execution, function, result_layout, 0,
        NULL, 0, &target->as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) {
        return status;
    }
    line = mmay_allocate(allocator, line_length);
    if (line == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (text->length != 0) {
        memcpy(line, text->bytes, text->length);
    }
    line[text->length] = '\n';
    output_status = mmay_test_output_publish(
        &options.execution->test_output,
        options.stdout_sink,
        line,
        line_length
    );
    mmay_deallocate(allocator, line, line_length);
    if (output_status == MMAY_TEST_OUTPUT_OK) {
        return MMAY_MIR_INTERPRET_OK;
    }
    if (output_status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (output_status == MMAY_TEST_OUTPUT_LIMIT_REACHED) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (output_status != MMAY_TEST_OUTPUT_CAPABILITY_DENIED
        && output_status != MMAY_TEST_OUTPUT_WRITE_FAILED) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    return mmay_mir_execute_io_error(
        function, result_layout, error_layout,
        output_status == MMAY_TEST_OUTPUT_CAPABILITY_DENIED
            ? MMAY_IO_STDOUT_CAPABILITY_DENIED
            : MMAY_IO_STDOUT_WRITE_FAILED,
        options.execution, target
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
