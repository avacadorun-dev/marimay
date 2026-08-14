#include "mir_internal.h"
#include "ruda/base/utf8.h"
RudaMirVerifyStatus ruda_mir_verify_managed_layouts(
    const RudaMirFunction *function,
    RudaMap *managed_types,
    size_t *work
) {
    size_t block_index = 0;
    {
        size_t verified_string_bytes = 0;
        for (block_index = 0; block_index < function->string_constants.length;
            block_index += 1) {
            const RudaMirStringConstant *constant = ruda_mir_string_constant(
                function,
                (RudaMirStringId)block_index
            );
            if (constant == NULL || constant->value.data == NULL
                || !ruda_utf8_validate(
                    (const char *)constant->value.data,
                    constant->value.length,
                    NULL
                ) || !ruda_size_add(
                    verified_string_bytes,
                    constant->value.length,
                    &verified_string_bytes
                )) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!ruda_mir_consume_verify_work(
                    work,
                    constant->value.length,
                    function->options.max_verify_work
                ) || !ruda_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
        }
        if (verified_string_bytes != function->string_bytes) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
    }
    for (block_index = 0; block_index < function->records.length;
        block_index += 1) {
        const RudaMirRecord *record = ruda_mir_record(
            function,
            (RudaMirRecordId)block_index
        );
        size_t field_end = 0;
        size_t field_index = 0;
        if (record == NULL || record->type < RUDA_TYPE_FIRST_DYNAMIC
            || !ruda_size_add(
                record->field_start,
                record->field_count,
                &field_end
            ) || field_end > function->record_fields.length) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (ruda_map_get_const(
                managed_types,
                &record->type,
                sizeof(record->type)
            ) != NULL) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        {
            RudaMirRecordId record_id = (RudaMirRecordId)block_index;
            if (!ruda_map_put(
                    managed_types,
                    &record->type,
                    sizeof(record->type),
                    &record_id
                )) {
                return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
            }
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
        for (field_index = record->field_start; field_index < field_end;
            field_index += 1) {
            const RudaMirRecordField *field = ruda_vector_at_const(
                &function->record_fields,
                field_index
            );
            if (field == NULL || field->type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!ruda_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }
    for (block_index = 0; block_index < function->record_constructions.length;
        block_index += 1) {
        const RudaMirRecordConstruction *construction =
            ruda_mir_record_construction(
                function,
                (RudaMirRecordConstructionId)block_index
            );
        const RudaMirRecord *record = construction != NULL
            ? ruda_mir_record(function, construction->record)
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || record == NULL
            || construction->argument_count != record->field_count
            || !ruda_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->record_arguments.length) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaMirRecordArgument *argument = ruda_mir_record_argument(
                function,
                construction->argument_start + argument_index
            );
            const RudaMirRecordField *field = ruda_mir_record_field(
                function,
                record,
                argument_index
            );
            if (argument == NULL || field == NULL
                || argument->register_id >= function->register_types.length
                || argument->type != field->type
                || ruda_mir_register_type(
                    function,
                    argument->register_id
                ) != argument->type) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!ruda_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }
    for (block_index = 0; block_index < function->enums.length;
        block_index += 1) {
        const RudaMirEnum *enum_layout = ruda_mir_enum(
            function,
            (RudaMirEnumId)block_index
        );
        size_t variant_end = 0;
        size_t payload_end = 0;
        size_t payload_cursor = 0;
        size_t variant_index = 0;
        if (enum_layout == NULL
            || enum_layout->type < RUDA_TYPE_FIRST_DYNAMIC
            || !ruda_size_add(
                enum_layout->variant_start,
                enum_layout->variant_count,
                &variant_end
            ) || variant_end > function->enum_variants.length
            || !ruda_size_add(
                enum_layout->payload_start,
                enum_layout->payload_count,
                &payload_end
            ) || payload_end > function->enum_payloads.length) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (ruda_map_get_const(
                managed_types,
                &enum_layout->type,
                sizeof(enum_layout->type)
            ) != NULL) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        {
            RudaMirEnumId enum_id = (RudaMirEnumId)block_index;
            if (!ruda_map_put(
                    managed_types,
                    &enum_layout->type,
                    sizeof(enum_layout->type),
                    &enum_id
                )) {
                return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
            }
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
        for (variant_index = 0; variant_index < enum_layout->variant_count;
            variant_index += 1) {
            const RudaMirEnumVariant *variant = ruda_mir_enum_variant(
                function,
                enum_layout,
                variant_index
            );
            size_t local_payload_end = 0;
            size_t payload_index = 0;
            if (variant == NULL
                || variant->kind < RUDA_MIR_ENUM_VARIANT_UNIT
                || variant->kind > RUDA_MIR_ENUM_VARIANT_NAMED
                || variant->payload_start
                    != enum_layout->payload_start + payload_cursor
                || !ruda_size_add(
                    payload_cursor,
                    variant->payload_count,
                    &local_payload_end
                ) || local_payload_end > enum_layout->payload_count
                || (variant->kind == RUDA_MIR_ENUM_VARIANT_UNIT
                    && variant->payload_count != 0)) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            for (payload_index = 0; payload_index < variant->payload_count;
                payload_index += 1) {
                const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
                    function,
                    variant,
                    payload_index
                );
                if (payload == NULL || payload->type == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                }
                if (!ruda_mir_consume_verify_work(
                        work,
                        1,
                        function->options.max_verify_work
                    )) {
                    return RUDA_MIR_VERIFY_LIMIT_REACHED;
                }
            }
            payload_cursor = local_payload_end;
        }
        if (payload_cursor != enum_layout->payload_count) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
    }
    for (block_index = 0; block_index < function->arrays.length;
        block_index += 1) {
        const RudaMirArray *array = ruda_mir_array(
            function,
            (RudaMirArrayId)block_index
        );
        RudaTypeId layout_id = (RudaTypeId)block_index;
        if (array == NULL || array->type < RUDA_TYPE_FIRST_DYNAMIC
            || array->element_type == RUDA_TYPE_INVALID) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (ruda_map_get_const(
                managed_types,
                &array->type,
                sizeof(array->type)
            ) != NULL) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_map_put(
                managed_types,
                &array->type,
                sizeof(array->type),
                &layout_id
            )) {
            return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
    }
    for (block_index = 0; block_index < function->enum_constructions.length;
        block_index += 1) {
        const RudaMirEnumConstruction *construction =
            ruda_mir_enum_construction(
                function,
                (RudaMirEnumConstructionId)block_index
            );
        const RudaMirEnum *enum_layout = construction != NULL
            ? ruda_mir_enum(function, construction->enum_id)
            : NULL;
        const RudaMirEnumVariant *variant = construction != NULL
            ? ruda_mir_enum_variant(
                function,
                enum_layout,
                construction->variant
            )
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || enum_layout == NULL || variant == NULL
            || construction->argument_count != variant->payload_count
            || !ruda_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->enum_arguments.length) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaMirEnumArgument *argument = ruda_mir_enum_argument(
                function,
                construction->argument_start + argument_index
            );
            const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
                function,
                variant,
                argument_index
            );
            if (argument == NULL || payload == NULL
                || argument->register_id >= function->register_types.length
                || argument->type != payload->type
                || ruda_mir_register_type(
                    function,
                    argument->register_id
                ) != argument->type) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!ruda_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }
    for (block_index = 0; block_index < function->array_constructions.length;
        block_index += 1) {
        const RudaMirArrayConstruction *construction =
            ruda_mir_array_construction(
                function,
                (RudaMirArrayConstructionId)block_index
            );
        const RudaMirArray *array = construction != NULL
            ? ruda_mir_array(function, construction->array)
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || array == NULL || !ruda_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->array_arguments.length) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = construction->argument_start;
            argument_index < argument_end; argument_index += 1) {
            const RudaMirArrayArgument *argument = ruda_mir_array_argument(
                function,
                argument_index
            );
            if (argument == NULL
                || argument->register_id >= function->register_types.length
                || argument->type != array->element_type
                || ruda_mir_register_type(
                    function,
                    argument->register_id
                ) != argument->type) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (!ruda_mir_consume_verify_work(
                    work,
                    1,
                    function->options.max_verify_work
                )) {
                return RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
        }
    }

    return RUDA_MIR_VERIFY_OK;
}
