#include "mir_internal.h"

static bool mmay_mir_fs_variant_payloads(
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

static const MarimayMirEnum *mmay_mir_fs_result(
    const MarimayMirFunction *function,
    MarimayTypeId result_type,
    MarimayTypeId ok_type,
    MarimayTypeId *error_type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        const MarimayMirEnumVariant *ok = NULL;
        const MarimayMirEnumVariant *error = NULL;
        const MarimayMirEnumPayload *payload = NULL;
        if (layout == NULL || layout->type != result_type
            || layout->variant_count != 2) continue;
        ok = mmay_mir_enum_variant(function, layout, 0);
        error = mmay_mir_enum_variant(function, layout, 1);
        if (ok == NULL || error == NULL
            || ok->payload_count != (ok_type == MMAY_TYPE_VOID ? 0U : 1U)
            || error->payload_count != 1) return NULL;
        if (ok_type != MMAY_TYPE_VOID) {
            payload = mmay_mir_enum_payload(function, ok, 0);
            if (payload == NULL || (ok_type != MMAY_TYPE_INVALID
                    && payload->type != ok_type)) return NULL;
        }
        payload = mmay_mir_enum_payload(function, error, 0);
        if (payload == NULL || payload->type < MMAY_TYPE_FIRST_DYNAMIC) {
            return NULL;
        }
        *error_type = payload->type;
        return layout;
    }
    return NULL;
}

static bool mmay_mir_fs_error_layout(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    static const MarimayTypeId denied[] = {
        MMAY_TYPE_STRING, MMAY_TYPE_STRING
    };
    static const MarimayTypeId integer[] = {MMAY_TYPE_I64};
    size_t index = 0;
    const MarimayMirEnum *layout = NULL;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *candidate = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (candidate != NULL && candidate->type == type) {
            layout = candidate;
            break;
        }
    }
    if (layout == NULL || layout->variant_count != 17
        || !mmay_mir_fs_variant_payloads(function, layout, 0, denied, 2)
        || !mmay_mir_fs_variant_payloads(function, layout, 1, integer, 1)
        || !mmay_mir_fs_variant_payloads(function, layout, 2, integer, 1)
        || !mmay_mir_fs_variant_payloads(function, layout, 14, integer, 1)
        || !mmay_mir_fs_variant_payloads(function, layout, 15, integer, 1)) {
        return false;
    }
    for (index = 3; index <= 13; index += 1) {
        if (!mmay_mir_fs_variant_payloads(function, layout, index, NULL, 0)) {
            return false;
        }
    }
    return mmay_mir_fs_variant_payloads(function, layout, 16, NULL, 0);
}

static bool mmay_mir_path_error_layout(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    static const MarimayTypeId integer[] = {MMAY_TYPE_I64};
    size_t index = 0;
    const MarimayMirEnum *layout = NULL;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *candidate = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (candidate != NULL && candidate->type == type) {
            layout = candidate;
            break;
        }
    }
    return layout != NULL && layout->variant_count == 4
        && mmay_mir_fs_variant_payloads(function, layout, 0, NULL, 0)
        && mmay_mir_fs_variant_payloads(function, layout, 1, NULL, 0)
        && mmay_mir_fs_variant_payloads(function, layout, 2, NULL, 0)
        && mmay_mir_fs_variant_payloads(function, layout, 3, integer, 1);
}

static bool mmay_mir_fs_array_element(
    const MarimayMirFunction *function,
    MarimayTypeId array_type,
    MarimayTypeId element_type
) {
    size_t index = 0;
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_mir_array(
            function, (MarimayMirArrayId)index
        );
        if (array != NULL && array->type == array_type) {
            return array->element_type == element_type;
        }
    }
    return false;
}

MarimayMirVerifyStatus mmay_mir_verify_fs_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId target = mmay_mir_register_type(function, instruction->target);
    MarimayTypeId first = mmay_mir_register_type(function, instruction->first);
    MarimayTypeId second = instruction->kind == MMAY_MIR_FS_BINARY
        ? mmay_mir_register_type(function, instruction->second)
        : MMAY_TYPE_INVALID;
    MarimayTypeId ok_type = MMAY_TYPE_VOID;
    MarimayTypeId error_type = MMAY_TYPE_INVALID;
    const MarimayMirEnum *result = NULL;
    if (first == MMAY_TYPE_INVALID
        || (instruction->kind == MMAY_MIR_FS_BINARY
            && second == MMAY_TYPE_INVALID)) {
        return MMAY_MIR_VERIFY_INVALID_REGISTER;
    }
    if (instruction->fs_operation == MMAY_MIR_PATH_IS_ABSOLUTE) {
        return instruction->kind == MMAY_MIR_PATH
                && first == MMAY_TYPE_STRING && target == MMAY_TYPE_BOOL
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->fs_operation == MMAY_MIR_PATH_NORMALIZE) {
        result = mmay_mir_fs_result(
            function, target, MMAY_TYPE_STRING, &error_type
        );
        return instruction->kind == MMAY_MIR_PATH
                && first == MMAY_TYPE_STRING && result != NULL
                && mmay_mir_path_error_layout(function, error_type)
            ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
    }
    switch (instruction->fs_operation) {
        case MMAY_MIR_FS_OPEN_READ:
            ok_type = MMAY_TYPE_I64;
            if (instruction->kind != MMAY_MIR_FS_UNARY
                || first != MMAY_TYPE_STRING) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_MIR_FS_READ_LINE: {
            if (instruction->kind != MMAY_MIR_FS_BINARY || first != MMAY_TYPE_I64 || second != MMAY_TYPE_I64)
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            result = mmay_mir_fs_result(function, target, MMAY_TYPE_INVALID, &error_type);
            const MarimayMirEnumVariant *ok = result != NULL ? mmay_mir_enum_variant(function, result, 0) : NULL;
            const MarimayMirEnumPayload *payload = ok != NULL ? mmay_mir_enum_payload(function, ok, 0) : NULL;
            const MarimayTypeId string_type = MMAY_TYPE_STRING;
            for (size_t i = 0; payload != NULL && i < function->enums.length; i += 1) {
                const MarimayMirEnum *option = mmay_mir_enum(function, (MarimayMirEnumId)i);
                if (option != NULL && option->type == payload->type && option->variant_count == 2
                    && mmay_mir_fs_variant_payloads(function, option, 0, &string_type, 1)
                    && mmay_mir_fs_variant_payloads(function, option, 1, NULL, 0)
                    && mmay_mir_fs_error_layout(function, error_type)) return MMAY_MIR_VERIFY_OK;
            }
            return MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        case MMAY_MIR_FS_READ:
        case MMAY_MIR_FS_READ_ALL:
            if (instruction->kind != MMAY_MIR_FS_BINARY
                || second != MMAY_TYPE_I64
                || (instruction->fs_operation == MMAY_MIR_FS_READ
                    ? first != MMAY_TYPE_I64 : first != MMAY_TYPE_STRING)) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            result = mmay_mir_fs_result(
                function, target, MMAY_TYPE_INVALID, &error_type
            );
            if (result == NULL) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            {
                const MarimayMirEnumVariant *ok = mmay_mir_enum_variant(
                    function, result, 0
                );
                const MarimayMirEnumPayload *payload = ok != NULL
                    ? mmay_mir_enum_payload(function, ok, 0) : NULL;
                if (payload == NULL || !mmay_mir_fs_array_element(
                        function, payload->type, MMAY_TYPE_U8
                    )) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            return mmay_mir_fs_error_layout(function, error_type)
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_FS_CLOSE:
            if (instruction->kind != MMAY_MIR_FS_UNARY
                || first != MMAY_TYPE_I64) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_MIR_FS_CLOSE_BEST_EFFORT:
            return instruction->kind == MMAY_MIR_FS_UNARY
                    && first == MMAY_TYPE_I64 && target == MMAY_TYPE_VOID
                ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
        case MMAY_MIR_FS_WRITE_ATOMIC:
            if (instruction->kind != MMAY_MIR_FS_BINARY
                || first != MMAY_TYPE_STRING
                || !mmay_mir_fs_array_element(
                    function, second, MMAY_TYPE_U8
                )) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_MIR_FS_METADATA:
            if (instruction->kind != MMAY_MIR_FS_UNARY
                || first != MMAY_TYPE_STRING) return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            ok_type = MMAY_TYPE_INVALID;
            break;
        case MMAY_MIR_FS_READ_DIRECTORY:
            if (instruction->kind != MMAY_MIR_FS_BINARY
                || first != MMAY_TYPE_STRING || second != MMAY_TYPE_I64) {
                return MMAY_MIR_VERIFY_TYPE_MISMATCH;
            }
            ok_type = MMAY_TYPE_INVALID;
            break;
        case MMAY_MIR_PATH_NORMALIZE:
        case MMAY_MIR_PATH_IS_ABSOLUTE:
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        default:
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (ok_type == MMAY_TYPE_INVALID) {
        size_t index = 0;
        for (index = 0; index < function->enums.length; index += 1) {
            const MarimayMirEnum *candidate = mmay_mir_enum(
                function, (MarimayMirEnumId)index
            );
            const MarimayMirEnumVariant *ok = NULL;
            const MarimayMirEnumPayload *payload = NULL;
            if (candidate == NULL || candidate->type != target
                || candidate->variant_count != 2) continue;
            ok = mmay_mir_enum_variant(function, candidate, 0);
            payload = ok != NULL && ok->payload_count == 1
                ? mmay_mir_enum_payload(function, ok, 0) : NULL;
            if (payload != NULL) ok_type = payload->type;
            break;
        }
    }
    result = mmay_mir_fs_result(function, target, ok_type, &error_type);
    return result != NULL && mmay_mir_fs_error_layout(function, error_type)
        ? MMAY_MIR_VERIFY_OK : MMAY_MIR_VERIFY_TYPE_MISMATCH;
}
