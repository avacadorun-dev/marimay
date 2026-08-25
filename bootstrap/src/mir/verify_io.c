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
        function, io_error, instruction->kind == MMAY_MIR_IO_PRINTLN ? 1 : 2
    );
    const MarimayMirEnumPayload *capability_payload = mmay_mir_enum_payload(
        function, capability, 0
    );
    MarimaySymbolId result_symbol;
    MarimaySymbolId error_symbol;
    MarimayTypeId target_type = mmay_mir_register_type(
        function, instruction->target
    );
    MarimayTypeId source_type = mmay_mir_register_type(
        function, instruction->first
    );
    bool takes_text = instruction->kind != MMAY_MIR_IO_FLUSH
        && instruction->kind != MMAY_MIR_IO_READ_LINE;
    if (target_type == MMAY_TYPE_INVALID
        || (takes_text && source_type == MMAY_TYPE_INVALID)) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (!mmay_mir_io_symbol("std", "prelude", "Result", &result_symbol)
        || !mmay_mir_io_symbol(
            "std/io", "std/io/io.my", "Error", &error_symbol
        )) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (instruction->kind == MMAY_MIR_IO_READ_LINE) {
        const MarimayMirEnum *option = mmay_mir_enum(
            function, instruction->option_enum_id
        );
        return source_type == MMAY_TYPE_I64 && result_layout != NULL
            && target_type == result_layout->type && option != NULL
            && result_layout->variant_count == 2
            && ok != NULL && ok->payload_count == 1
            && error != NULL && error->payload_count == 1
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if ((takes_text && source_type != MMAY_TYPE_STRING)
        || (instruction->kind == MMAY_MIR_IO_READ_LINE
            && source_type != MMAY_TYPE_I64) || result_layout == NULL
        || target_type != result_layout->type || ok == NULL || error == NULL
        || error_payload == NULL || io_error == NULL
        || capability == NULL || write_failed == NULL
        || capability_payload == NULL
        || !mmay_symbol_id_equal(result_layout->symbol, result_symbol)
        || !mmay_symbol_id_equal(io_error->symbol, error_symbol)
        || result_layout->variant_count != 2
        || ok->kind != MMAY_MIR_ENUM_VARIANT_POSITIONAL
        || (instruction->kind == MMAY_MIR_IO_READ_LINE
            ? ok->payload_count != 1 : ok->payload_count != 0)
        || error->kind != MMAY_MIR_ENUM_VARIANT_POSITIONAL
        || error->payload_count != 1
        || io_error->variant_count != (instruction->kind == MMAY_MIR_IO_PRINTLN
            ? 2 : 6)
        || capability->kind != MMAY_MIR_ENUM_VARIANT_NAMED
        || capability->payload_count != 1
        || capability_payload->type != MMAY_TYPE_STRING
        || write_failed->kind != MMAY_MIR_ENUM_VARIANT_UNIT
        || write_failed->payload_count != 0
        || (instruction->kind == MMAY_MIR_IO_WRITE
            && (instruction->io_operation < MMAY_MIR_IO_STDOUT_WRITE
                || instruction->io_operation > MMAY_MIR_IO_STDERR_LINE))
        || (instruction->kind == MMAY_MIR_IO_FLUSH
            && instruction->io_operation != MMAY_MIR_IO_STDOUT_FLUSH
            && instruction->io_operation != MMAY_MIR_IO_STDERR_FLUSH)
        || (instruction->kind == MMAY_MIR_IO_READ_LINE
            && (instruction->option_enum_id == MMAY_MIR_NO_ENUM
                || ok->payload_count != 1))) {
        return MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    return MMAY_MIR_VERIFY_OK;
}
