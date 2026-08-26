#include "bytecode_internal.h"


static const MarimayBytecodeEnumVariant *mmay_bytecode_io_variant(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant
) {
    return function != NULL && layout != NULL && variant < layout->variant_count
        && layout->variant_start <= function->enum_variants.length
        && variant < function->enum_variants.length - layout->variant_start
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + variant
        ) : NULL;
}

static const MarimayBytecodeEnumPayload *mmay_bytecode_io_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant,
    size_t field
) {
    size_t relative = 0;
    if (function == NULL || layout == NULL || variant == NULL
        || field >= variant->payload_count
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

MarimayBytecodeVerifyStatus mmay_bytecode_verify_io_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t text = instruction->operands[1].unsigned_value;
    const MarimayBytecodeType *target_type = target < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)target) : NULL;
    const MarimayBytecodeType *text_type = text < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)text) : NULL;
    uint32_t result_id = target_type != NULL
        ? mmay_bytecode_register_layout(function, (uint32_t)target)
        : MMAY_BYTECODE_NO_LAYOUT;
    const MarimayBytecodeEnumLayout *result_layout = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumVariant *ok = mmay_bytecode_io_variant(
        function, result_layout, 0
    );
    const MarimayBytecodeEnumVariant *error = mmay_bytecode_io_variant(
        function, result_layout, 1
    );
    const MarimayBytecodeEnumPayload *error_payload = mmay_bytecode_io_payload(
        function, result_layout, error, 0
    );
    const MarimayBytecodeEnumLayout *io_error = error_payload != NULL
            && error_payload->type == MMAY_BYTECODE_TYPE_ENUM
        ? mmay_bytecode_enum_layout(function, error_payload->layout) : NULL;
    const MarimayBytecodeEnumVariant *capability = mmay_bytecode_io_variant(
        function, io_error, 0
    );
    const MarimayBytecodeEnumVariant *write_failed = mmay_bytecode_io_variant(
        function, io_error, io_error != NULL && io_error->variant_count == 2
            ? 1 : 2
    );
    const MarimayBytecodeEnumVariant *read_failed = mmay_bytecode_io_variant(
        function, io_error, 1
    );
    const MarimayBytecodeEnumVariant *flush_failed = mmay_bytecode_io_variant(
        function, io_error, 3
    );
    const MarimayBytecodeEnumVariant *invalid_utf8 = mmay_bytecode_io_variant(
        function, io_error, 4
    );
    const MarimayBytecodeEnumVariant *line_too_long = mmay_bytecode_io_variant(
        function, io_error, 5
    );
    const MarimayBytecodeEnumVariant *input_too_long =
        mmay_bytecode_io_variant(function, io_error, 6);
    const MarimayBytecodeEnumLayout *option = instruction->opcode
            == MMAY_OPCODE_IO_READ_LINE
        ? mmay_bytecode_enum_layout(
            function, (uint32_t)instruction->operands[2].signed_value
        ) : NULL;
    const MarimayBytecodeEnumVariant *some = mmay_bytecode_io_variant(
        function, option, 0
    );
    const MarimayBytecodeEnumVariant *none = mmay_bytecode_io_variant(
        function, option, 1
    );
    const MarimayBytecodeEnumPayload *ok_payload = mmay_bytecode_io_payload(
        function, result_layout, ok, 0
    );
    const MarimayBytecodeEnumPayload *some_payload = mmay_bytecode_io_payload(
        function, option, some, 0
    );
    const MarimayBytecodeEnumPayload *capability_payload =
        mmay_bytecode_io_payload(function, io_error, capability, 0);
    const MarimayBytecodeEnumPayload *line_limit_payload =
        mmay_bytecode_io_payload(function, io_error, line_too_long, 0);
    const MarimayBytecodeEnumPayload *input_limit_payload =
        mmay_bytecode_io_payload(function, io_error, input_too_long, 0);
    uint32_t source_element_layout = MMAY_BYTECODE_NO_LAYOUT;
    MarimayBytecodeType source_element = text_type != NULL
            && mmay_bytecode_type_is_array(*text_type)
        ? mmay_bytecode_function_array_element_type(
            function, *text_type,
            mmay_bytecode_register_layout(function, (uint32_t)text),
            &source_element_layout
        ) : MMAY_BYTECODE_TYPE_INVALID;
    int64_t read_array_wire = instruction->opcode == MMAY_OPCODE_IO_READ_BYTES
        ? instruction->operands[2].signed_value : -1;
    const MarimayBytecodeArrayType *read_array = read_array_wire >= 0
        ? mmay_bytecode_array_type(function, (uint32_t)read_array_wire) : NULL;
    if (instruction->opcode == MMAY_OPCODE_IO_FLUSH) text_type = NULL;
    if (target_type == NULL || (instruction->opcode != MMAY_OPCODE_IO_FLUSH && text_type == NULL)) {
        return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_WRITE
        && (instruction->operands[2].signed_value < 0
            || instruction->operands[2].signed_value > 3)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_WRITE_BYTES
        && instruction->operands[2].signed_value != 0
        && instruction->operands[2].signed_value != 2) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_FLUSH
        && instruction->operands[1].signed_value != 4
        && instruction->operands[1].signed_value != 5) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_READ_LINE
        && (instruction->operands[2].signed_value < 0 || text_type == NULL
            || *text_type != MMAY_BYTECODE_TYPE_I64 || option == NULL
            || option->variant_count != 2 || ok == NULL
            || ok->payload_count != 1 || ok_payload == NULL
            || ok_payload->layout != (uint32_t)instruction->operands[2].signed_value
            || none == NULL || none->payload_count != 0
            || some == NULL || some->payload_count != 1 || some_payload == NULL
            || some_payload->type != MMAY_BYTECODE_TYPE_STRING)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_READ_BYTES
        && (text_type == NULL || *text_type != MMAY_BYTECODE_TYPE_I64
            || read_array == NULL
            || read_array->element_type != MMAY_BYTECODE_TYPE_U8
            || read_array->element_layout != MMAY_BYTECODE_NO_LAYOUT
            || ok == NULL || ok->payload_count != 1 || ok_payload == NULL
            || ok_payload->type != MMAY_BYTECODE_TYPE_ARRAY
            || ok_payload->layout != (uint32_t)read_array_wire)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_IO_WRITE_BYTES
        && (text_type == NULL || source_element != MMAY_BYTECODE_TYPE_U8
            || source_element_layout != MMAY_BYTECODE_NO_LAYOUT)) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    return *target_type == MMAY_BYTECODE_TYPE_ENUM
            && (instruction->opcode == MMAY_OPCODE_IO_FLUSH
                || instruction->opcode == MMAY_OPCODE_IO_READ_LINE
                || instruction->opcode == MMAY_OPCODE_IO_READ_BYTES
                || instruction->opcode == MMAY_OPCODE_IO_WRITE_BYTES
                || *text_type == MMAY_BYTECODE_TYPE_STRING)
            && result_layout != NULL && result_layout->variant_count == 2
            && ok != NULL && ((instruction->opcode == MMAY_OPCODE_IO_READ_LINE
                    || instruction->opcode == MMAY_OPCODE_IO_READ_BYTES)
                ? ok->payload_count == 1 : ok->payload_count == 0)
            && error != NULL && error->payload_count == 1
            && error_payload != NULL
            && io_error != NULL && (instruction->opcode == MMAY_OPCODE_IO_PRINTLN
                ? (io_error->variant_count == 2
                    || io_error->variant_count == 6
                    || io_error->variant_count == 7)
                : (instruction->opcode == MMAY_OPCODE_IO_READ_BYTES
                    || instruction->opcode == MMAY_OPCODE_IO_WRITE_BYTES)
                ? io_error->variant_count == 7
                : (io_error->variant_count == 6
                    || io_error->variant_count == 7))
            && capability != NULL && capability->payload_count == 1
            && capability_payload != NULL
            && capability_payload->type == MMAY_BYTECODE_TYPE_STRING
            && capability_payload->layout == MMAY_BYTECODE_NO_LAYOUT
            && write_failed != NULL && write_failed->payload_count == 0
            && (io_error->variant_count < 6
                || (read_failed != NULL && read_failed->payload_count == 0
                    && flush_failed != NULL
                    && flush_failed->payload_count == 0
                    && invalid_utf8 != NULL
                    && invalid_utf8->payload_count == 0
                    && line_too_long != NULL
                    && line_too_long->payload_count == 1
                    && line_limit_payload != NULL
                    && line_limit_payload->type == MMAY_BYTECODE_TYPE_I64
                    && line_limit_payload->layout == MMAY_BYTECODE_NO_LAYOUT
                    && (io_error->variant_count != 7
                        || (input_too_long != NULL
                            && input_too_long->payload_count == 1
                            && input_limit_payload != NULL
                            && input_limit_payload->type
                                == MMAY_BYTECODE_TYPE_I64
                            && input_limit_payload->layout
                                == MMAY_BYTECODE_NO_LAYOUT))))
        ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
}
