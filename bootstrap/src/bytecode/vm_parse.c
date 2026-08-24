#include "vm_internal.h"

#include <string.h>

#include "mmay/base/integer_text.h"

static const MarimayBytecodeEnumPayload *mmay_bytecode_parse_run_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant_index,
    size_t field
) {
    const MarimayBytecodeEnumVariant *variant;
    size_t relative;
    if (function == NULL || layout == NULL
        || variant_index >= layout->variant_count
        || layout->variant_start > function->enum_variants.length
        || variant_index >= function->enum_variants.length
            - layout->variant_start) {
        return NULL;
    }
    variant = mmay_vector_at_const(
        &function->enum_variants, layout->variant_start + variant_index
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

MarimayBytecodeRunStatus mmay_bytecode_run_parse_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
) {
    size_t target_index = (size_t)instruction->operands[0].unsigned_value;
    size_t text_index = (size_t)instruction->operands[1].unsigned_value;
    size_t radix_index = (size_t)instruction->operands[2].unsigned_value;
    uint32_t result_id = mmay_bytecode_register_layout(
        function, (uint32_t)target_index
    );
    const MarimayBytecodeEnumLayout *result_layout = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumPayload *error_payload =
        mmay_bytecode_parse_run_payload(function, result_layout, 1, 0);
    const void *result_descriptor = NULL;
    const void *error_descriptor = NULL;
    const unsigned char *text = NULL;
    size_t length = 0;
    int64_t parsed = 0;
    size_t offset = 0;
    MarimayIntegerParseStatus parse_status;
    MarimayManagedCell payload;
    MarimayManagedReference error = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[1] = {&error};
    MarimayManagedRootFrame roots;
    MarimayManagedStatus managed_status;
    MarimayBytecodeRunStatus status;
    size_t variant;
    if (execution == NULL || !execution->initialized || function == NULL
        || result_layout == NULL || error_payload == NULL
        || error_payload->type != MMAY_BYTECODE_TYPE_ENUM
        || !mmay_bytecode_execution_string_bytes(
            execution, registers[text_index], &text, &length
        )) {
        return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    status = mmay_bytecode_run_nominal_descriptor(
        execution, function, MMAY_BYTECODE_TYPE_ENUM,
        result_id, &result_descriptor
    );
    if (status == MMAY_BYTECODE_RUN_OK) {
        status = mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            error_payload->layout, &error_descriptor
        );
    }
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    parse_status = mmay_parse_i64(
        text, length, registers[radix_index].as.i64, &parsed, &offset
    );
    memset(&payload, 0, sizeof(payload));
    if (parse_status == MMAY_INTEGER_PARSE_OK) {
        payload.f64_bits = (uint64_t)parsed;
        return mmay_bytecode_run_managed_status(mmay_managed_enum_new(
            &execution->managed_heap, result_descriptor, 0,
            &payload, 1, &registers[target_index].as.managed_reference
        ));
    }
    variant = parse_status == MMAY_INTEGER_PARSE_EMPTY ? 0U
        : parse_status == MMAY_INTEGER_PARSE_INVALID_DIGIT ? 1U
        : parse_status == MMAY_INTEGER_PARSE_OUT_OF_RANGE ? 2U : 3U;
    memset(&roots, 0, sizeof(roots));
    managed_status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 1
    );
    if (managed_status != MMAY_MANAGED_OK) {
        return mmay_bytecode_run_managed_status(managed_status);
    }
    if (variant == 1U) {
        payload.f64_bits = (uint64_t)offset;
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, error_descriptor, variant,
            &payload, 1, &error
        );
    } else {
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, error_descriptor, variant,
            NULL, 0, &error
        );
    }
    if (managed_status == MMAY_MANAGED_OK) {
        payload.reference = error;
        managed_status = mmay_managed_enum_new(
            &execution->managed_heap, result_descriptor, 1,
            &payload, 1, &registers[target_index].as.managed_reference
        );
    }
    {
        MarimayManagedStatus pop_status = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (managed_status == MMAY_MANAGED_OK) managed_status = pop_status;
    }
    return mmay_bytecode_run_managed_status(managed_status);
}
