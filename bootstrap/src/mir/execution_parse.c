#include "mir_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/integer_text.h"

static const MarimayMirEnum *mmay_mir_parse_execution_enum_by_type(
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

MarimayMirInterpretStatus mmay_mir_execute_parse_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *text_value,
    const MarimayMirValue *radix_value,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    const MarimayMirEnum *result_layout = mmay_mir_enum(
        function, instruction != NULL ? instruction->enum_id : MMAY_MIR_NO_ENUM
    );
    const MarimayMirEnumVariant *error_variant = mmay_mir_enum_variant(
        function, result_layout, 1
    );
    const MarimayMirEnumPayload *error_payload = mmay_mir_enum_payload(
        function, error_variant, 0
    );
    const MarimayMirEnum *error_layout = error_payload != NULL
        ? mmay_mir_parse_execution_enum_by_type(
            function, error_payload->type
        ) : NULL;
    const MarimayMirManagedString *text = options.execution != NULL
        && text_value != NULL
        ? mmay_mir_execution_string_value(options.execution, *text_value)
        : NULL;
    MarimayIntegerParseStatus parse_status;
    MarimayMirValue payload;
    MarimayMirValue error;
    int64_t parsed = 0;
    size_t offset = 0;
    size_t variant;
    MarimayMirInterpretStatus status;
    if (function == NULL || instruction == NULL || radix_value == NULL
        || target == NULL || options.execution == NULL || text == NULL
        || result_layout == NULL || error_layout == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    parse_status = mmay_parse_i64(
        text->bytes, text->length, radix_value->as.i64,
        &parsed, &offset
    );
    memset(&payload, 0, sizeof(payload));
    if (parse_status == MMAY_INTEGER_PARSE_OK) {
        payload.type = MMAY_TYPE_I64;
        payload.as.i64 = parsed;
        return mmay_mir_execution_new_enum(
            options.execution, function, result_layout, 0,
            &payload, 1, &target->as.reference
        );
    }
    variant = parse_status == MMAY_INTEGER_PARSE_EMPTY ? 0U
        : parse_status == MMAY_INTEGER_PARSE_INVALID_DIGIT ? 1U
        : parse_status == MMAY_INTEGER_PARSE_OUT_OF_RANGE ? 2U : 3U;
    memset(&error, 0, sizeof(error));
    error.type = error_layout->type;
    if (variant == 1U) {
        if (offset > (size_t)INT64_MAX) {
            return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        }
        payload.type = MMAY_TYPE_I64;
        payload.as.i64 = (int64_t)offset;
        status = mmay_mir_execution_new_enum(
            options.execution, function, error_layout, variant,
            &payload, 1, &error.as.reference
        );
    } else {
        status = mmay_mir_execution_new_enum(
            options.execution, function, error_layout, variant,
            NULL, 0, &error.as.reference
        );
    }
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    return mmay_mir_execution_new_enum(
        options.execution, function, result_layout, 1,
        &error, 1, &target->as.reference
    );
}
