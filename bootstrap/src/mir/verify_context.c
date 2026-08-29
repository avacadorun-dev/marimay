#include "mir_internal.h"
#include "context_internal.h"

static const MarimayMirEnum *mmay_mir_context_enum(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static bool mmay_mir_context_payloads(
    const MarimayMirFunction *function,
    const MarimayMirEnum *layout,
    size_t variant,
    const MarimayTypeId *types,
    size_t count
) {
    const MarimayMirEnumVariant *item = mmay_mir_enum_variant(
        function, layout, variant
    );
    size_t index = 0;
    if (item == NULL || item->payload_count != count) return false;
    for (index = 0; index < count; index += 1) {
        const MarimayMirEnumPayload *payload = mmay_mir_enum_payload(
            function, item, index
        );
        if (payload == NULL || payload->type != types[index]) return false;
    }
    return true;
}

static bool mmay_mir_context_error(
    const MarimayMirFunction *function,
    MarimayTypeId type,
    bool environment
) {
    static const MarimayTypeId string_payload[] = {MMAY_TYPE_STRING};
    static const MarimayTypeId integer_payload[] = {MMAY_TYPE_I64};
    const MarimayMirEnum *layout = mmay_mir_context_enum(function, type);
    if (layout == NULL) return false;
    if (environment) {
        return layout->variant_count == 5
            && mmay_mir_context_payloads(function, layout, 0, NULL, 0)
            && mmay_mir_context_payloads(
                function, layout, 1, string_payload, 1
            ) && mmay_mir_context_payloads(
                function, layout, 2, integer_payload, 1
            ) && mmay_mir_context_payloads(
                function, layout, 3, integer_payload, 1
            ) && mmay_mir_context_payloads(function, layout, 4, NULL, 0);
    }
    return layout->variant_count == 4
        && mmay_mir_context_payloads(function, layout, 0, NULL, 0)
        && mmay_mir_context_payloads(function, layout, 1, NULL, 0)
        && mmay_mir_context_payloads(
            function, layout, 2, integer_payload, 1
        ) && mmay_mir_context_payloads(function, layout, 3, NULL, 0);
}

static bool mmay_mir_context_result(
    const MarimayMirFunction *function,
    MarimayTypeId result_type,
    bool environment
) {
    const MarimayMirEnum *result = mmay_mir_context_enum(function, result_type);
    const MarimayMirEnumVariant *ok = result != NULL
        ? mmay_mir_enum_variant(function, result, 0) : NULL;
    const MarimayMirEnumVariant *error = result != NULL
        ? mmay_mir_enum_variant(function, result, 1) : NULL;
    const MarimayMirEnumPayload *ok_payload = ok != NULL
        && ok->payload_count == 1
        ? mmay_mir_enum_payload(function, ok, 0) : NULL;
    const MarimayMirEnumPayload *error_payload = error != NULL
        && error->payload_count == 1
        ? mmay_mir_enum_payload(function, error, 0) : NULL;
    if (result == NULL || result->variant_count != 2 || ok_payload == NULL
        || error_payload == NULL
        || !mmay_mir_context_error(
            function, error_payload->type, environment
        )) {
        return false;
    }
    if (!environment) return ok_payload->type == MMAY_TYPE_STRING;
    {
        const MarimayMirEnum *option = mmay_mir_context_enum(
            function, ok_payload->type
        );
        static const MarimayTypeId string_payload[] = {MMAY_TYPE_STRING};
        return option != NULL && option->variant_count == 2
            && mmay_mir_context_payloads(
                function, option, 0, string_payload, 1
            )
            && mmay_mir_context_payloads(
                function, option, 1, NULL, 0
            );
    }
}

MarimayMirVerifyStatus mmay_mir_verify_context_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId target = mmay_mir_register_type(
        function, instruction->target
    );
    if (instruction->kind == MMAY_MIR_ENV_READ) {
        MarimayTypeId source = mmay_mir_register_type(
            function, instruction->first
        );
        if (source == MMAY_TYPE_INVALID) {
            return MMAY_MIR_VERIFY_INVALID_REGISTER;
        }
        return instruction->context_operation == MMAY_MIR_CONTEXT_ENV_GET
                && source == MMAY_TYPE_STRING
                && mmay_mir_context_result(function, target, true)
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->kind != MMAY_MIR_PROCESS_CONTEXT
        || (instruction->context_operation
                != MMAY_MIR_CONTEXT_CURRENT_DIRECTORY
            && instruction->context_operation
                != MMAY_MIR_CONTEXT_EXECUTABLE_PATH)) {
        return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    return mmay_mir_context_result(function, target, false)
        ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
}
