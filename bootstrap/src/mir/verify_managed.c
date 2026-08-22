#include "mir_internal.h"
#include "mmay/base/utf8.h"
MarimayMirVerifyStatus mmay_mir_verify_managed_layouts(
    const MarimayMirFunction *function,
    MarimayMap *managed_types,
    size_t *work
) {
    size_t block_index = 0;
    {
        size_t verified_string_bytes = 0;
        for (block_index = 0; block_index < function->string_constants.length;
            block_index += 1) {
            const MarimayMirStringConstant *constant = mmay_mir_string_constant(
                function,
                (MarimayMirStringId)block_index
            );
            if (constant == NULL || constant->value.data == NULL
                || !mmay_utf8_validate(
                    (const char *)constant->value.data,
                    constant->value.length,
                    NULL
                ) || !mmay_size_add(
                    verified_string_bytes,
                    constant->value.length,
                    &verified_string_bytes
                )) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_mir_consume_verify_work(
                    work,
                    constant->value.length,
                    function->options.max_verify_work
                ) || !mmay_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return MMAY_MIR_VERIFY_LIMIT_REACHED;
            }
        }
        if (verified_string_bytes != function->string_bytes) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
    }
    for (block_index = 0; block_index < function->records.length;
        block_index += 1) {
        const MarimayMirRecord *record = mmay_mir_record(
            function,
            (MarimayMirRecordId)block_index
        );
        size_t field_end = 0;
        size_t field_index = 0;
        if (record == NULL || record->type < MMAY_TYPE_FIRST_DYNAMIC
            || !mmay_size_add(
                record->field_start,
                record->field_count,
                &field_end
            ) || field_end > function->record_fields.length) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (mmay_map_get_const(
                managed_types,
                &record->type,
                sizeof(record->type)
            ) != NULL) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        {
            MarimayMirManagedLayoutInfo info = {
                MMAY_MIR_MANAGED_LAYOUT_RECORD,
                (uint32_t)block_index
            };
            if (!mmay_map_put(
                    managed_types,
                    &record->type,
                    sizeof(record->type),
                    &info
                )) {
                return MMAY_MIR_VERIFY_ALLOCATION_FAILED;
            }
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
        for (field_index = record->field_start; field_index < field_end;
            field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &function->record_fields,
                field_index
            );
            if (field == NULL || field->type == MMAY_TYPE_INVALID) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return MMAY_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }
    for (block_index = 0; block_index < function->record_constructions.length;
        block_index += 1) {
        const MarimayMirRecordConstruction *construction =
            mmay_mir_record_construction(
                function,
                (MarimayMirRecordConstructionId)block_index
            );
        const MarimayMirRecord *record = construction != NULL
            ? mmay_mir_record(function, construction->record)
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || record == NULL
            || construction->argument_count != record->field_count
            || !mmay_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->record_arguments.length) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayMirRecordArgument *argument = mmay_mir_record_argument(
                function,
                construction->argument_start + argument_index
            );
            const MarimayMirRecordField *field = mmay_mir_record_field(
                function,
                record,
                argument_index
            );
            if (argument == NULL || field == NULL
                || argument->register_id >= function->register_types.length
                || argument->type != field->type
                || mmay_mir_register_type(
                    function,
                    argument->register_id
                ) != argument->type) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return MMAY_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }
    for (block_index = 0; block_index < function->enums.length;
        block_index += 1) {
        const MarimayMirEnum *enum_layout = mmay_mir_enum(
            function,
            (MarimayMirEnumId)block_index
        );
        size_t variant_end = 0;
        size_t payload_end = 0;
        size_t payload_cursor = 0;
        size_t variant_index = 0;
        if (enum_layout == NULL
            || enum_layout->type < MMAY_TYPE_FIRST_DYNAMIC
            || !mmay_size_add(
                enum_layout->variant_start,
                enum_layout->variant_count,
                &variant_end
            ) || variant_end > function->enum_variants.length
            || !mmay_size_add(
                enum_layout->payload_start,
                enum_layout->payload_count,
                &payload_end
            ) || payload_end > function->enum_payloads.length) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (mmay_map_get_const(
                managed_types,
                &enum_layout->type,
                sizeof(enum_layout->type)
            ) != NULL) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        {
            MarimayMirManagedLayoutInfo info = {
                MMAY_MIR_MANAGED_LAYOUT_ENUM,
                (uint32_t)block_index
            };
            if (!mmay_map_put(
                    managed_types,
                    &enum_layout->type,
                    sizeof(enum_layout->type),
                    &info
                )) {
                return MMAY_MIR_VERIFY_ALLOCATION_FAILED;
            }
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
        for (variant_index = 0; variant_index < enum_layout->variant_count;
            variant_index += 1) {
            const MarimayMirEnumVariant *variant = mmay_mir_enum_variant(
                function,
                enum_layout,
                variant_index
            );
            size_t local_payload_end = 0;
            size_t payload_index = 0;
            if (variant == NULL
                || variant->kind < MMAY_MIR_ENUM_VARIANT_UNIT
                || variant->kind > MMAY_MIR_ENUM_VARIANT_NAMED
                || variant->payload_start
                    != enum_layout->payload_start + payload_cursor
                || !mmay_size_add(
                    payload_cursor,
                    variant->payload_count,
                    &local_payload_end
                ) || local_payload_end > enum_layout->payload_count
                || (variant->kind == MMAY_MIR_ENUM_VARIANT_UNIT
                    && variant->payload_count != 0)) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            for (payload_index = 0; payload_index < variant->payload_count;
                payload_index += 1) {
                const MarimayMirEnumPayload *payload = mmay_mir_enum_payload(
                    function,
                    variant,
                    payload_index
                );
                if (payload == NULL || payload->type == MMAY_TYPE_INVALID) {
                    return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
                }
                if (!mmay_mir_consume_verify_work(
                        work,
                        1,
                        function->options.max_verify_work
                    )) {
                    return MMAY_MIR_VERIFY_LIMIT_REACHED;
                }
            }
            payload_cursor = local_payload_end;
        }
        if (payload_cursor != enum_layout->payload_count) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
    }
    for (block_index = 0; block_index < function->arrays.length;
        block_index += 1) {
        const MarimayMirArray *array = mmay_mir_array(
            function,
            (MarimayMirArrayId)block_index
        );
        MarimayMirManagedLayoutInfo info = {
            MMAY_MIR_MANAGED_LAYOUT_ARRAY,
            (uint32_t)block_index
        };
        if (array == NULL || array->type < MMAY_TYPE_FIRST_DYNAMIC
            || array->element_type == MMAY_TYPE_INVALID) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (mmay_map_get_const(
                managed_types,
                &array->type,
                sizeof(array->type)
            ) != NULL) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_map_put(
            managed_types,
            &array->type,
            sizeof(array->type),
            &info
        )) {
            return MMAY_MIR_VERIFY_ALLOCATION_FAILED;
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
    }
    for (block_index = 0; block_index < function->enum_constructions.length;
        block_index += 1) {
        const MarimayMirEnumConstruction *construction =
            mmay_mir_enum_construction(
                function,
                (MarimayMirEnumConstructionId)block_index
            );
        const MarimayMirEnum *enum_layout = construction != NULL
            ? mmay_mir_enum(function, construction->enum_id)
            : NULL;
        const MarimayMirEnumVariant *variant = construction != NULL
            ? mmay_mir_enum_variant(
                function,
                enum_layout,
                construction->variant
            )
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || enum_layout == NULL || variant == NULL
            || construction->argument_count != variant->payload_count
            || !mmay_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->enum_arguments.length) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayMirEnumArgument *argument = mmay_mir_enum_argument(
                function,
                construction->argument_start + argument_index
            );
            const MarimayMirEnumPayload *payload = mmay_mir_enum_payload(
                function,
                variant,
                argument_index
            );
            if (argument == NULL || payload == NULL
                || argument->register_id >= function->register_types.length
                || argument->type != payload->type
                || mmay_mir_register_type(
                    function,
                    argument->register_id
                ) != argument->type) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return MMAY_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }
    for (block_index = 0; block_index < function->array_constructions.length;
        block_index += 1) {
        const MarimayMirArrayConstruction *construction =
            mmay_mir_array_construction(
                function,
                (MarimayMirArrayConstructionId)block_index
            );
        const MarimayMirArray *array = construction != NULL
            ? mmay_mir_array(function, construction->array)
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || array == NULL || !mmay_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->array_arguments.length) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = construction->argument_start;
            argument_index < argument_end; argument_index += 1) {
            const MarimayMirArrayArgument *argument = mmay_mir_array_argument(
                function,
                argument_index
            );
            if (argument == NULL
                || argument->register_id >= function->register_types.length
                || argument->type != array->element_type
                || mmay_mir_register_type(
                    function,
                    argument->register_id
                ) != argument->type) {
                return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!mmay_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return MMAY_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }

    return MMAY_MIR_VERIFY_OK;
}
