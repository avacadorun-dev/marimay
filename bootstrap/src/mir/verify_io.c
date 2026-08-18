#include "mir_internal.h"

static bool ruda_mir_io_symbol(
    const char *package_name,
    const char *source_path,
    const char *declaration_name,
    RudaSymbolId *symbol
) {
    return ruda_symbol_id_from_parts(
        package_name, source_path, declaration_name, 0, symbol
    );
}

static const RudaMirEnum *ruda_mir_io_enum_by_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; function != NULL && index < function->enums.length;
        index += 1) {
        const RudaMirEnum *layout = ruda_mir_enum(
            function, (RudaMirEnumId)index
        );
        if (layout != NULL && layout->type == type) {
            return layout;
        }
    }
    return NULL;
}

RudaMirVerifyStatus ruda_mir_verify_io_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirEnum *result_layout = ruda_mir_enum(
        function, instruction->enum_id
    );
    const RudaMirEnumVariant *ok = ruda_mir_enum_variant(
        function, result_layout, 0
    );
    const RudaMirEnumVariant *error = ruda_mir_enum_variant(
        function, result_layout, 1
    );
    const RudaMirEnumPayload *error_payload = ruda_mir_enum_payload(
        function, error, 0
    );
    const RudaMirEnum *io_error = error_payload != NULL
        ? ruda_mir_io_enum_by_type(function, error_payload->type) : NULL;
    const RudaMirEnumVariant *capability = ruda_mir_enum_variant(
        function, io_error, 0
    );
    const RudaMirEnumVariant *write_failed = ruda_mir_enum_variant(
        function, io_error, 1
    );
    const RudaMirEnumPayload *capability_payload = ruda_mir_enum_payload(
        function, capability, 0
    );
    RudaSymbolId result_symbol;
    RudaSymbolId error_symbol;
    RudaTypeId target_type = ruda_mir_register_type(
        function, instruction->target
    );
    RudaTypeId source_type = ruda_mir_register_type(
        function, instruction->first
    );
    if (target_type == RUDA_TYPE_INVALID
        || source_type == RUDA_TYPE_INVALID) {
        return RUDA_MIR_VERIFY_INVALID_REGISTER;
    }
    if (!ruda_mir_io_symbol("std", "prelude", "Result", &result_symbol)
        || !ruda_mir_io_symbol(
            "std/io", "std/io/io.ruda", "Error", &error_symbol
        )) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (source_type != RUDA_TYPE_STRING || result_layout == NULL
        || target_type != result_layout->type || ok == NULL || error == NULL
        || error_payload == NULL || io_error == NULL
        || capability == NULL || write_failed == NULL
        || capability_payload == NULL
        || !ruda_symbol_id_equal(result_layout->symbol, result_symbol)
        || !ruda_symbol_id_equal(io_error->symbol, error_symbol)
        || result_layout->variant_count != 2
        || ok->kind != RUDA_MIR_ENUM_VARIANT_POSITIONAL
        || ok->payload_count != 0
        || error->kind != RUDA_MIR_ENUM_VARIANT_POSITIONAL
        || error->payload_count != 1
        || io_error->variant_count != 2
        || capability->kind != RUDA_MIR_ENUM_VARIANT_NAMED
        || capability->payload_count != 1
        || capability_payload->type != RUDA_TYPE_STRING
        || write_failed->kind != RUDA_MIR_ENUM_VARIANT_UNIT
        || write_failed->payload_count != 0) {
        return RUDA_MIR_VERIFY_TYPE_MISMATCH;
    }
    return RUDA_MIR_VERIFY_OK;
}
