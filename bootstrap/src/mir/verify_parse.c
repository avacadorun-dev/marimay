#include "mir_internal.h"

static const MarimayMirEnum *mmay_mir_parse_enum_by_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index;
    for (index = 0; function != NULL && index < function->enums.length;
        index += 1U) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static bool mmay_mir_parse_symbol(
    const char *package_name,
    const char *source_path,
    const char *name,
    MarimaySymbolId *symbol
) {
    return mmay_symbol_id_from_parts(
        package_name, source_path, name, 0, symbol
    );
}

MarimayMirVerifyStatus mmay_mir_verify_parse_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnum *result_layout = mmay_mir_enum(
        function, instruction != NULL ? instruction->enum_id : MMAY_MIR_NO_ENUM
    );
    const MarimayMirEnumVariant *ok = mmay_mir_enum_variant(
        function, result_layout, 0
    );
    const MarimayMirEnumVariant *error = mmay_mir_enum_variant(
        function, result_layout, 1
    );
    const MarimayMirEnumPayload *ok_payload = mmay_mir_enum_payload(
        function, ok, 0
    );
    const MarimayMirEnumPayload *error_payload = mmay_mir_enum_payload(
        function, error, 0
    );
    const MarimayMirEnum *parse_error = error_payload != NULL
        ? mmay_mir_parse_enum_by_type(function, error_payload->type) : NULL;
    const MarimayMirEnumVariant *empty = mmay_mir_enum_variant(
        function, parse_error, 0
    );
    const MarimayMirEnumVariant *invalid_digit = mmay_mir_enum_variant(
        function, parse_error, 1
    );
    const MarimayMirEnumVariant *out_of_range = mmay_mir_enum_variant(
        function, parse_error, 2
    );
    const MarimayMirEnumVariant *invalid_radix = mmay_mir_enum_variant(
        function, parse_error, 3
    );
    const MarimayMirEnumPayload *offset = mmay_mir_enum_payload(
        function, invalid_digit, 0
    );
    MarimaySymbolId result_symbol;
    MarimaySymbolId error_symbol;
    MarimayTypeId target_type;
    MarimayTypeId text_type;
    MarimayTypeId radix_type;
    if (function == NULL || instruction == NULL) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    target_type = mmay_mir_register_type(function, instruction->target);
    text_type = mmay_mir_register_type(function, instruction->first);
    radix_type = mmay_mir_register_type(function, instruction->second);
    if (target_type == MMAY_TYPE_INVALID || text_type == MMAY_TYPE_INVALID
        || radix_type == MMAY_TYPE_INVALID) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (!mmay_mir_parse_symbol("std", "prelude", "Result", &result_symbol)
        || !mmay_mir_parse_symbol(
            "std/convert", "std/convert/convert.my", "ParseError",
            &error_symbol
        )) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    return text_type == MMAY_TYPE_STRING && radix_type == MMAY_TYPE_I64
            && result_layout != NULL && target_type == result_layout->type
            && mmay_symbol_id_equal(result_layout->symbol, result_symbol)
            && result_layout->variant_count == 2
            && ok != NULL && ok->kind == MMAY_MIR_ENUM_VARIANT_POSITIONAL
            && ok->payload_count == 1 && ok_payload != NULL
            && ok_payload->type == MMAY_TYPE_I64
            && error != NULL
            && error->kind == MMAY_MIR_ENUM_VARIANT_POSITIONAL
            && error->payload_count == 1 && error_payload != NULL
            && parse_error != NULL
            && mmay_symbol_id_equal(parse_error->symbol, error_symbol)
            && parse_error->variant_count == 4
            && empty != NULL && empty->kind == MMAY_MIR_ENUM_VARIANT_UNIT
            && empty->payload_count == 0
            && invalid_digit != NULL
            && invalid_digit->kind == MMAY_MIR_ENUM_VARIANT_NAMED
            && invalid_digit->payload_count == 1 && offset != NULL
            && offset->type == MMAY_TYPE_I64
            && out_of_range != NULL
            && out_of_range->kind == MMAY_MIR_ENUM_VARIANT_UNIT
            && out_of_range->payload_count == 0
            && invalid_radix != NULL
            && invalid_radix->kind == MMAY_MIR_ENUM_VARIANT_UNIT
            && invalid_radix->payload_count == 0
        ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
}
