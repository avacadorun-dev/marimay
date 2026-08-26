#include "mir_internal.h"


static bool mmay_mir_io_symbol(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    MarimaySymbolId *symbol
) {
    return mmay_symbol_id_from_parts(
        package_name, source_path, declaration_name, 0, symbol
    );
}

static const MarimayMirEnum *mmay_mir_io_enum_by_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; function != NULL && index < function->enums.length;
        index += 1) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (layout != NULL && layout->type == type) {
            return layout;
        }
    }
    return NULL;
}

static const MarimayMirArray *mmay_mir_io_array_by_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; function != NULL && index < function->arrays.length;
        index += 1) {
        const MarimayMirArray *array = mmay_mir_array(
            function, (MarimayMirArrayId)index
        );
        if (array != NULL && array->type == type) return array;
    }
    return NULL;
}

MarimayMirVerifyStatus mmay_mir_verify_io_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnum *result_layout = mmay_mir_enum(
        function, instruction->enum_id
    );
    const MarimayMirEnumVariant *ok = mmay_mir_enum_variant(
        function, result_layout, 0
    );
    const MarimayMirEnumVariant *error = mmay_mir_enum_variant(
        function, result_layout, 1
    );
    const MarimayMirEnumPayload *error_payload = mmay_mir_enum_payload(
        function, error, 0
    );
    const MarimayMirEnum *io_error = error_payload != NULL
        ? mmay_mir_io_enum_by_type(function, error_payload->type) : NULL;
    const MarimayMirEnumVariant *capability = mmay_mir_enum_variant(
        function, io_error, 0
    );
    const MarimayMirEnumVariant *write_failed = mmay_mir_enum_variant(
        function, io_error, io_error != NULL && io_error->variant_count == 2
            ? 1 : 2
    );
    const MarimayMirEnumVariant *read_failed = mmay_mir_enum_variant(
        function, io_error, 1
    );
    const MarimayMirEnumVariant *flush_failed = mmay_mir_enum_variant(
        function, io_error, 3
    );
    const MarimayMirEnumVariant *invalid_utf8 = mmay_mir_enum_variant(
        function, io_error, 4
    );
    const MarimayMirEnumVariant *line_too_long = mmay_mir_enum_variant(
        function, io_error, 5
    );
    const MarimayMirEnumVariant *input_too_long = mmay_mir_enum_variant(
        function, io_error, 6
    );
    const MarimayMirEnumPayload *capability_payload = mmay_mir_enum_payload(
        function, capability, 0
    );
    const MarimayMirEnumPayload *line_limit_payload = mmay_mir_enum_payload(
        function, line_too_long, 0
    );
    const MarimayMirEnumPayload *input_limit_payload = mmay_mir_enum_payload(
        function, input_too_long, 0
    );
    const MarimayMirEnum *option = instruction->kind == MMAY_MIR_IO_READ_LINE
        ? mmay_mir_enum(function, instruction->option_enum_id) : NULL;
    const MarimayMirEnumVariant *some = mmay_mir_enum_variant(
        function, option, 0
    );
    const MarimayMirEnumVariant *none = mmay_mir_enum_variant(
        function, option, 1
    );
    const MarimayMirEnumPayload *ok_payload = mmay_mir_enum_payload(
        function, ok, 0
    );
    const MarimayMirEnumPayload *some_payload = mmay_mir_enum_payload(
        function, some, 0
    );
    MarimaySymbolId result_symbol;
    MarimaySymbolId error_symbol;
    MarimayTypeId target_type = mmay_mir_register_type(
        function, instruction->target
    );
    MarimayTypeId source_type = mmay_mir_register_type(
        function, instruction->first
    );
    const MarimayMirArray *bytes = instruction->kind == MMAY_MIR_IO_READ_BYTES
        ? mmay_mir_array(function, instruction->array)
        : mmay_mir_io_array_by_type(function, source_type);
    bool takes_text = instruction->kind == MMAY_MIR_IO_PRINTLN
        || instruction->kind == MMAY_MIR_IO_WRITE;
    bool takes_bytes = instruction->kind == MMAY_MIR_IO_WRITE_BYTES;
    bool takes_limit = instruction->kind == MMAY_MIR_IO_READ_LINE
        || instruction->kind == MMAY_MIR_IO_READ_BYTES;
    if (target_type == MMAY_TYPE_INVALID
        || ((takes_text || takes_bytes || takes_limit)
            && source_type == MMAY_TYPE_INVALID)) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (!mmay_mir_io_symbol("std", "prelude", "Result", &result_symbol)
        || !mmay_mir_io_symbol(
            "std/io", "std/io/io.my", "Error", &error_symbol
        )) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if ((takes_text && source_type != MMAY_TYPE_STRING)
        || (takes_limit && source_type != MMAY_TYPE_I64)
        || (takes_bytes && (bytes == NULL
            || bytes->element_type != MMAY_TYPE_U8)) || result_layout == NULL
        || target_type != result_layout->type || ok == NULL || error == NULL
        || error_payload == NULL || io_error == NULL
        || capability == NULL || write_failed == NULL
        || capability_payload == NULL
        || !mmay_symbol_id_equal(result_layout->symbol, result_symbol)
        || !mmay_symbol_id_equal(io_error->symbol, error_symbol)
        || result_layout->variant_count != 2
        || ok->kind != MMAY_MIR_ENUM_VARIANT_POSITIONAL
        || (instruction->kind == MMAY_MIR_IO_READ_LINE
            ? (ok->payload_count != 1 || ok_payload == NULL
                || option == NULL || ok_payload->type != option->type)
            : instruction->kind == MMAY_MIR_IO_READ_BYTES
            ? (ok->payload_count != 1 || ok_payload == NULL
                || bytes == NULL || bytes->element_type != MMAY_TYPE_U8
                || ok_payload->type != bytes->type)
            : ok->payload_count != 0)
        || error->kind != MMAY_MIR_ENUM_VARIANT_POSITIONAL
        || error->payload_count != 1
        || ((instruction->kind == MMAY_MIR_IO_READ_BYTES
                || instruction->kind == MMAY_MIR_IO_WRITE_BYTES)
            ? io_error->variant_count != 7
            : instruction->kind == MMAY_MIR_IO_PRINTLN
            ? (io_error->variant_count != 2 && io_error->variant_count != 6
                && io_error->variant_count != 7)
            : (io_error->variant_count != 6
                && io_error->variant_count != 7))
        || capability->kind != MMAY_MIR_ENUM_VARIANT_NAMED
        || capability->payload_count != 1
        || capability_payload->type != MMAY_TYPE_STRING
        || write_failed->kind != MMAY_MIR_ENUM_VARIANT_UNIT
        || write_failed->payload_count != 0
        || (io_error->variant_count >= 6
            && (read_failed == NULL || read_failed->payload_count != 0
                || flush_failed == NULL || flush_failed->payload_count != 0
                || invalid_utf8 == NULL || invalid_utf8->payload_count != 0
                || line_too_long == NULL
                || line_too_long->kind != MMAY_MIR_ENUM_VARIANT_NAMED
                || line_too_long->payload_count != 1
                || line_limit_payload == NULL
                || line_limit_payload->type != MMAY_TYPE_I64
                || (io_error->variant_count == 7
                    && (input_too_long == NULL
                        || input_too_long->kind
                            != MMAY_MIR_ENUM_VARIANT_NAMED
                        || input_too_long->payload_count != 1
                        || input_limit_payload == NULL
                        || input_limit_payload->type != MMAY_TYPE_I64))))
        || (instruction->kind == MMAY_MIR_IO_WRITE
            && (instruction->io_operation < MMAY_MIR_IO_STDOUT_WRITE
                || instruction->io_operation > MMAY_MIR_IO_STDERR_LINE))
        || (instruction->kind == MMAY_MIR_IO_FLUSH
            && instruction->io_operation != MMAY_MIR_IO_STDOUT_FLUSH
            && instruction->io_operation != MMAY_MIR_IO_STDERR_FLUSH)
        || (instruction->kind == MMAY_MIR_IO_READ_LINE
            && (instruction->option_enum_id == MMAY_MIR_NO_ENUM
                || option->variant_count != 2 || some == NULL
                || some->payload_count != 1 || some_payload == NULL
                || some_payload->type != MMAY_TYPE_STRING || none == NULL
                || none->payload_count != 0))
        || (instruction->kind == MMAY_MIR_IO_READ_BYTES
            && (instruction->array == MMAY_MIR_NO_ARRAY || bytes == NULL))
        || (instruction->kind == MMAY_MIR_IO_WRITE_BYTES
            && instruction->io_operation != MMAY_MIR_IO_STDOUT_WRITE
            && instruction->io_operation != MMAY_MIR_IO_STDERR_WRITE)) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    return MMAY_MIR_VERIFY_OK;
}
