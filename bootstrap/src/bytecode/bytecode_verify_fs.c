#include "bytecode_internal.h"

static const MarimayBytecodeEnumVariant *mmay_bytecode_fs_variant(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t index
) {
    return layout != NULL && index < layout->variant_count
            && layout->variant_start <= function->enum_variants.length
            && index < function->enum_variants.length - layout->variant_start
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + index
        ) : NULL;
}

static const MarimayBytecodeEnumPayload *mmay_bytecode_fs_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    const MarimayBytecodeEnumVariant *variant,
    size_t index
) {
    size_t relative = 0;
    if (layout == NULL || variant == NULL || index >= variant->payload_count
        || !mmay_size_add(variant->payload_start, index, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static bool mmay_bytecode_fs_payloads(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant_index,
    const MarimayBytecodeType *types,
    size_t count
) {
    const MarimayBytecodeEnumVariant *variant = mmay_bytecode_fs_variant(
        function, layout, variant_index
    );
    size_t index = 0;
    if (variant == NULL || variant->payload_count != count) return false;
    for (index = 0; index < count; index += 1) {
        const MarimayBytecodeEnumPayload *payload = mmay_bytecode_fs_payload(
            function, layout, variant, index
        );
        if (payload == NULL || payload->type != types[index]
            || payload->layout != MMAY_BYTECODE_NO_LAYOUT) return false;
    }
    return true;
}

static bool mmay_bytecode_fs_path_error(
    const MarimayBytecodeFunction *function,
    uint32_t layout_id
) {
    static const MarimayBytecodeType integer[] = {MMAY_BYTECODE_TYPE_I64};
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, layout_id
    );
    return layout != NULL && layout->variant_count == 4
        && mmay_bytecode_fs_payloads(function, layout, 0, NULL, 0)
        && mmay_bytecode_fs_payloads(function, layout, 1, NULL, 0)
        && mmay_bytecode_fs_payloads(function, layout, 2, NULL, 0)
        && mmay_bytecode_fs_payloads(function, layout, 3, integer, 1);
}

static bool mmay_bytecode_fs_error(
    const MarimayBytecodeFunction *function,
    uint32_t layout_id
) {
    static const MarimayBytecodeType denied[] = {
        MMAY_BYTECODE_TYPE_STRING, MMAY_BYTECODE_TYPE_STRING
    };
    static const MarimayBytecodeType integer[] = {MMAY_BYTECODE_TYPE_I64};
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, layout_id
    );
    size_t index = 0;
    if (layout == NULL || layout->variant_count != 17
        || !mmay_bytecode_fs_payloads(function, layout, 0, denied, 2)
        || !mmay_bytecode_fs_payloads(function, layout, 1, integer, 1)
        || !mmay_bytecode_fs_payloads(function, layout, 2, integer, 1)
        || !mmay_bytecode_fs_payloads(function, layout, 14, integer, 1)
        || !mmay_bytecode_fs_payloads(function, layout, 15, integer, 1)
        || !mmay_bytecode_fs_payloads(function, layout, 16, NULL, 0)) {
        return false;
    }
    for (index = 3; index <= 13; index += 1) {
        if (!mmay_bytecode_fs_payloads(function, layout, index, NULL, 0)) {
            return false;
        }
    }
    return true;
}

static bool mmay_bytecode_fs_array_u8(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout
) {
    const MarimayBytecodeArrayType *array = type == MMAY_BYTECODE_TYPE_ARRAY
        ? mmay_bytecode_array_type(function, layout) : NULL;
    return array != NULL && array->element_type == MMAY_BYTECODE_TYPE_U8
        && array->element_layout == MMAY_BYTECODE_NO_LAYOUT;
}

static bool mmay_bytecode_fs_result(
    const MarimayBytecodeFunction *function,
    uint32_t result_id,
    MarimayBytecodeType ok_type,
    bool path_error,
    const MarimayBytecodeEnumPayload **ok_payload_out
) {
    const MarimayBytecodeEnumLayout *result = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumVariant *ok = mmay_bytecode_fs_variant(
        function, result, 0
    );
    const MarimayBytecodeEnumVariant *error = mmay_bytecode_fs_variant(
        function, result, 1
    );
    const MarimayBytecodeEnumPayload *ok_payload = ok != NULL
            && ok->payload_count == 1
        ? mmay_bytecode_fs_payload(function, result, ok, 0) : NULL;
    const MarimayBytecodeEnumPayload *error_payload = mmay_bytecode_fs_payload(
        function, result, error, 0
    );
    if (ok_payload_out != NULL) *ok_payload_out = ok_payload;
    return result != NULL && result->variant_count == 2 && ok != NULL
        && ok->payload_count == (ok_type == MMAY_BYTECODE_TYPE_VOID ? 0U : 1U)
        && (ok_type == MMAY_BYTECODE_TYPE_VOID
            || (ok_payload != NULL && (ok_type == MMAY_BYTECODE_TYPE_INVALID
                || (ok_payload->type == ok_type
                    && ok_payload->layout == MMAY_BYTECODE_NO_LAYOUT))))
        && error != NULL && error->payload_count == 1
        && error_payload != NULL
        && error_payload->type == MMAY_BYTECODE_TYPE_ENUM
        && (path_error
            ? mmay_bytecode_fs_path_error(function, error_payload->layout)
            : mmay_bytecode_fs_error(function, error_payload->layout));
}

static bool mmay_bytecode_fs_metadata_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumPayload *payload
) {
    const MarimayBytecodeRecordLayout *record = payload != NULL
            && payload->type == MMAY_BYTECODE_TYPE_RECORD
        ? mmay_bytecode_record_layout(function, payload->layout) : NULL;
    const MarimayBytecodeRecordField *kind = record != NULL
        ? mmay_bytecode_record_field(function, record, 0) : NULL;
    const MarimayBytecodeRecordField *size = record != NULL
        ? mmay_bytecode_record_field(function, record, 1) : NULL;
    const MarimayBytecodeEnumLayout *kind_layout = kind != NULL
            && kind->type == MMAY_BYTECODE_TYPE_ENUM
        ? mmay_bytecode_enum_layout(function, kind->layout) : NULL;
    return record != NULL && record->field_count == 2 && kind != NULL
        && size != NULL && size->type == MMAY_BYTECODE_TYPE_I64
        && size->layout == MMAY_BYTECODE_NO_LAYOUT && kind_layout != NULL
        && kind_layout->variant_count == 2
        && mmay_bytecode_fs_payloads(function, kind_layout, 0, NULL, 0)
        && mmay_bytecode_fs_payloads(function, kind_layout, 1, NULL, 0);
}

static bool mmay_bytecode_fs_directory_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumPayload *payload
) {
    const MarimayBytecodeArrayType *array = payload != NULL
            && payload->type == MMAY_BYTECODE_TYPE_ARRAY
        ? mmay_bytecode_array_type(function, payload->layout) : NULL;
    const MarimayBytecodeRecordLayout *record = array != NULL
            && array->element_type == MMAY_BYTECODE_TYPE_RECORD
        ? mmay_bytecode_record_layout(function, array->element_layout) : NULL;
    const MarimayBytecodeRecordField *name = record != NULL
        ? mmay_bytecode_record_field(function, record, 0) : NULL;
    const MarimayBytecodeRecordField *kind = record != NULL
        ? mmay_bytecode_record_field(function, record, 1) : NULL;
    const MarimayBytecodeEnumLayout *kind_layout = kind != NULL
            && kind->type == MMAY_BYTECODE_TYPE_ENUM
        ? mmay_bytecode_enum_layout(function, kind->layout) : NULL;
    size_t index = 0;
    if (record == NULL || record->field_count != 2 || name == NULL
        || name->type != MMAY_BYTECODE_TYPE_STRING
        || name->layout != MMAY_BYTECODE_NO_LAYOUT || kind_layout == NULL
        || kind_layout->variant_count != 4) return false;
    for (index = 0; index < 4; index += 1) {
        if (!mmay_bytecode_fs_payloads(
                function, kind_layout, index, NULL, 0
            )) return false;
    }
    return true;
}

bool mmay_bytecode_opcode_is_fs(MarimayOpcode opcode) {
    return (opcode >= MMAY_OPCODE_PATH_NORMALIZE
        && opcode <= MMAY_OPCODE_FS_READ_DIRECTORY) || opcode == MMAY_OPCODE_FS_READ_LINE;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_fs_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = instruction->operands[1].unsigned_value;
    uint64_t second = instruction->operands[2].unsigned_value;
    const MarimayBytecodeType *target_type = target < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)target) : NULL;
    const MarimayBytecodeType *first_type = first < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)first) : NULL;
    const MarimayBytecodeType *second_type = second < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, (size_t)second) : NULL;
    const MarimayBytecodeEnumPayload *ok_payload = NULL;
    uint32_t result_id = target_type != NULL
        ? mmay_bytecode_register_layout(function, (uint32_t)target)
        : MMAY_BYTECODE_NO_LAYOUT;
    bool binary = instruction->opcode == MMAY_OPCODE_FS_READ_LINE
        || instruction->opcode == MMAY_OPCODE_FS_READ
        || instruction->opcode == MMAY_OPCODE_FS_READ_ALL
        || instruction->opcode == MMAY_OPCODE_FS_WRITE_ATOMIC
        || instruction->opcode == MMAY_OPCODE_FS_READ_DIRECTORY;
    if (target_type == NULL || first_type == NULL
        || first >= function->register_types.length || assigned[first] == 0
        || (binary && (second_type == NULL || assigned[second] == 0))) {
        return target_type == NULL || first_type == NULL
                || (binary && second_type == NULL)
            ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
            : MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
    }
    if (instruction->opcode == MMAY_OPCODE_PATH_IS_ABSOLUTE) {
        return *first_type == MMAY_BYTECODE_TYPE_STRING
                && *target_type == MMAY_BYTECODE_TYPE_BOOL
            ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_PATH_NORMALIZE) {
        return *first_type == MMAY_BYTECODE_TYPE_STRING
                && *target_type == MMAY_BYTECODE_TYPE_ENUM
                && mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_STRING, true, NULL
                )
            ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == MMAY_OPCODE_FS_CLOSE_BEST_EFFORT) {
        return *first_type == MMAY_BYTECODE_TYPE_I64
                && *target_type == MMAY_BYTECODE_TYPE_VOID
            ? MMAY_BYTECODE_VERIFY_OK : MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (*target_type != MMAY_BYTECODE_TYPE_ENUM) {
        return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_FS_OPEN_READ:
            if (*first_type != MMAY_BYTECODE_TYPE_STRING
                || !mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_I64, false, NULL
                )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_FS_READ_LINE: {
            if (*first_type != MMAY_BYTECODE_TYPE_I64 || *second_type != MMAY_BYTECODE_TYPE_I64
                || !mmay_bytecode_fs_result(function, result_id, MMAY_BYTECODE_TYPE_INVALID, false, &ok_payload)
                || ok_payload == NULL || ok_payload->type != MMAY_BYTECODE_TYPE_ENUM)
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            const MarimayBytecodeEnumLayout *option = mmay_bytecode_enum_layout(function, ok_payload->layout);
            const MarimayBytecodeType string_type = MMAY_BYTECODE_TYPE_STRING;
            if (option == NULL || option->variant_count != 2
                || !mmay_bytecode_fs_payloads(function, option, 0, &string_type, 1)
                || !mmay_bytecode_fs_payloads(function, option, 1, NULL, 0)) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        }
        case MMAY_OPCODE_FS_READ:
        case MMAY_OPCODE_FS_READ_ALL:
            if (*first_type != (instruction->opcode == MMAY_OPCODE_FS_READ
                    ? MMAY_BYTECODE_TYPE_I64 : MMAY_BYTECODE_TYPE_STRING)
                || *second_type != MMAY_BYTECODE_TYPE_I64
                || !mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_INVALID,
                    false, &ok_payload
                ) || ok_payload == NULL
                || !mmay_bytecode_fs_array_u8(
                    function, ok_payload->type, ok_payload->layout
                )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_FS_CLOSE:
            if (*first_type != MMAY_BYTECODE_TYPE_I64
                || !mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_VOID, false, NULL
                )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_FS_WRITE_ATOMIC:
            if (*first_type != MMAY_BYTECODE_TYPE_STRING
                || !mmay_bytecode_fs_array_u8(
                    function, *second_type,
                    mmay_bytecode_register_layout(function, (uint32_t)second)
                ) || !mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_VOID, false, NULL
                )) return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case MMAY_OPCODE_FS_METADATA:
            if (*first_type != MMAY_BYTECODE_TYPE_STRING
                || !mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_INVALID,
                    false, &ok_payload
                ) || !mmay_bytecode_fs_metadata_payload(function, ok_payload)) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        case MMAY_OPCODE_FS_READ_DIRECTORY:
            if (*first_type != MMAY_BYTECODE_TYPE_STRING
                || *second_type != MMAY_BYTECODE_TYPE_I64
                || !mmay_bytecode_fs_result(
                    function, result_id, MMAY_BYTECODE_TYPE_INVALID,
                    false, &ok_payload
                ) || !mmay_bytecode_fs_directory_payload(function, ok_payload)) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        default:
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    return MMAY_BYTECODE_VERIFY_OK;
}
