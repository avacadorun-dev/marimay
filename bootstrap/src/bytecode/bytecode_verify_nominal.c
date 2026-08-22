#include "bytecode_internal.h"

#include <limits.h>

static bool mmay_bytecode_nominal_consume(
    size_t *used,
    size_t limit
) {
    size_t next = 0;
    if (used == NULL || !mmay_size_add(*used, 1, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_verify_record_metadata(
    const MarimayBytecodeFunction *function,
    size_t limit,
    size_t *work
) {
    size_t layout_index = 0;
    size_t expected_field_start = 0;
    size_t construction_index = 0;
    size_t expected_argument_start = 0;
    for (layout_index = 0; layout_index < function->record_layouts.length;
        layout_index += 1) {
        const MarimayBytecodeRecordLayout *layout = mmay_bytecode_record_layout(
            function,
            (MarimayBytecodeRecordId)layout_index
        );
        size_t field_index = 0;
        size_t field_end = 0;
        if (layout == NULL || layout->field_start != expected_field_start
            || !mmay_size_add(
                layout->field_start,
                layout->field_count,
                &field_end
            ) || field_end > function->record_fields.length) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_bytecode_nominal_consume(work, limit)) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (field_index = 0; field_index < layout->field_count;
            field_index += 1) {
            const MarimayBytecodeRecordField *field = mmay_bytecode_record_field(
                function,
                layout,
                field_index
            );
            if (field == NULL || !mmay_bytecode_type_is_register(field->type)
                || !mmay_bytecode_type_layout_is_valid(
                    function,
                    field->type,
                    field->layout
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!mmay_bytecode_nominal_consume(work, limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_field_start = field_end;
    }
    if (expected_field_start != function->record_fields.length) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (construction_index = 0;
        construction_index < function->record_constructions.length;
        construction_index += 1) {
        const MarimayBytecodeRecordConstruction *construction =
            mmay_bytecode_record_construction(
                function,
                (MarimayBytecodeRecordConstructionId)construction_index
            );
        const MarimayBytecodeRecordLayout *layout = construction != NULL
            ? mmay_bytecode_record_layout(function, construction->record)
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL
            || construction->argument_start != expected_argument_start
            || construction->argument_count != layout->field_count
            || !mmay_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->record_arguments.length) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_bytecode_nominal_consume(work, limit)) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayBytecodeRecordArgument *argument = mmay_vector_at_const(
                &function->record_arguments,
                construction->argument_start + argument_index
            );
            const MarimayBytecodeRecordField *field = mmay_bytecode_record_field(
                function,
                layout,
                argument_index
            );
            const MarimayBytecodeType *actual = argument != NULL
                    && argument->register_id < function->register_types.length
                ? mmay_vector_at_const(
                    &function->register_types,
                    argument->register_id
                )
                : NULL;
            if (argument == NULL || actual == NULL) {
                return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            if (field == NULL || argument->type != field->type
                || argument->layout != field->layout
                || *actual != argument->type
                || mmay_bytecode_register_layout(
                    function,
                    argument->register_id
                ) != argument->layout) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!mmay_bytecode_nominal_consume(work, limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_argument_start = argument_end;
    }
    return expected_argument_start == function->record_arguments.length
        ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_verify_enum_metadata(
    const MarimayBytecodeFunction *function,
    size_t limit,
    size_t *work
) {
    size_t layout_index = 0;
    size_t expected_variant_start = 0;
    size_t expected_payload_start = 0;
    size_t construction_index = 0;
    size_t expected_argument_start = 0;
    for (layout_index = 0; layout_index < function->enum_layouts.length;
        layout_index += 1) {
        const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
            function,
            (MarimayBytecodeEnumId)layout_index
        );
        size_t variant_end = 0;
        size_t payload_end = 0;
        size_t variant_index = 0;
        size_t payload_index = 0;
        size_t local_payload_start = 0;
        if (layout == NULL
            || layout->variant_start != expected_variant_start
            || layout->payload_start != expected_payload_start
            || !mmay_size_add(
                layout->variant_start,
                layout->variant_count,
                &variant_end
            ) || variant_end > function->enum_variants.length
            || !mmay_size_add(
                layout->payload_start,
                layout->payload_count,
                &payload_end
            ) || payload_end > function->enum_payloads.length) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_bytecode_nominal_consume(work, limit)) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (variant_index = 0; variant_index < layout->variant_count;
            variant_index += 1) {
            const MarimayBytecodeEnumVariant *variant = mmay_vector_at_const(
                &function->enum_variants,
                layout->variant_start + variant_index
            );
            size_t local_payload_end = 0;
            if (variant == NULL
                || variant->payload_start != local_payload_start
                || !mmay_size_add(
                    variant->payload_start,
                    variant->payload_count,
                    &local_payload_end
                ) || local_payload_end > layout->payload_count) {
                return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            local_payload_start = local_payload_end;
            if (!mmay_bytecode_nominal_consume(work, limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        if (local_payload_start != layout->payload_count) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        for (payload_index = 0; payload_index < layout->payload_count;
            payload_index += 1) {
            const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
                &function->enum_payloads,
                layout->payload_start + payload_index
            );
            if (payload == NULL
                || !mmay_bytecode_type_is_register(payload->type)
                || !mmay_bytecode_type_layout_is_valid(
                    function,
                    payload->type,
                    payload->layout
                )) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!mmay_bytecode_nominal_consume(work, limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_variant_start = variant_end;
        expected_payload_start = payload_end;
    }
    if (expected_variant_start != function->enum_variants.length
        || expected_payload_start != function->enum_payloads.length) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (construction_index = 0;
        construction_index < function->enum_constructions.length;
        construction_index += 1) {
        const MarimayBytecodeEnumConstruction *construction =
            mmay_bytecode_enum_construction(
                function,
                (MarimayBytecodeEnumConstructionId)construction_index
            );
        const MarimayBytecodeEnumLayout *layout = construction != NULL
            ? mmay_bytecode_enum_layout(function, construction->enum_id)
            : NULL;
        const MarimayBytecodeEnumVariant *variant = layout != NULL
                && construction->variant < layout->variant_count
            ? mmay_vector_at_const(
                &function->enum_variants,
                layout->variant_start + construction->variant
            )
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL || variant == NULL
            || construction->argument_start != expected_argument_start
            || construction->argument_count != variant->payload_count
            || !mmay_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->enum_arguments.length) {
            return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_bytecode_nominal_consume(work, limit)) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayBytecodeEnumArgument *argument = mmay_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + argument_index
            );
            const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
                &function->enum_payloads,
                layout->payload_start + variant->payload_start + argument_index
            );
            const MarimayBytecodeType *actual = argument != NULL
                    && argument->register_id < function->register_types.length
                ? mmay_vector_at_const(
                    &function->register_types,
                    argument->register_id
                )
                : NULL;
            if (argument == NULL || actual == NULL) {
                return MMAY_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            if (payload == NULL || argument->type != payload->type
                || argument->layout != payload->layout
                || *actual != argument->type
                || mmay_bytecode_register_layout(
                    function,
                    argument->register_id
                ) != argument->layout) {
                return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!mmay_bytecode_nominal_consume(work, limit)) {
                return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_argument_start = argument_end;
    }
    return expected_argument_start == function->enum_arguments.length
        ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

static MarimayBytecodeVerifyStatus mmay_bytecode_verify_interface_metadata(
    const MarimayBytecodeFunction *function,
    size_t limit,
    size_t *work
) {
    size_t layout_index = 0;
    size_t construction_index = 0;
    size_t expected_method_start = 0;
    for (layout_index = 0; layout_index < function->interface_layouts.length;
        layout_index += 1) {
        const MarimayBytecodeInterfaceLayout *layout =
            mmay_bytecode_interface_layout(
                function,
                (MarimayBytecodeInterfaceId)layout_index
            );
        if (layout == NULL || !mmay_bytecode_nominal_consume(work, limit)) {
            return layout == NULL
                ? MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT
                : MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
    }
    for (construction_index = 0;
        construction_index < function->interface_constructions.length;
        construction_index += 1) {
        const MarimayBytecodeInterfaceConstruction *construction =
            mmay_bytecode_interface_construction(
                function,
                (MarimayBytecodeInterfaceConstructionId)construction_index
            );
        const MarimayBytecodeInterfaceLayout *layout = construction != NULL
            ? mmay_bytecode_interface_layout(
                function,
                construction->interface_id
            )
            : NULL;
        const MarimayBytecodeType *actual = construction != NULL
                && construction->payload_register
                    < function->register_types.length
            ? mmay_vector_at_const(
                &function->register_types,
                construction->payload_register
            )
            : NULL;
        size_t method_end = 0;
        if (construction == NULL || layout == NULL || actual == NULL
            || construction->method_start != expected_method_start
            || construction->method_count != layout->method_count
            || !mmay_size_add(
                construction->method_start,
                construction->method_count,
                &method_end
            ) || method_end > function->interface_methods.length) {
            return actual == NULL
                ? MMAY_BYTECODE_VERIFY_INVALID_REGISTER
                : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (*actual != construction->payload_type
            || mmay_bytecode_register_layout(
                function,
                construction->payload_register
            ) != construction->payload_layout
            || !mmay_bytecode_type_layout_is_valid(
                function,
                construction->payload_type,
                construction->payload_layout
            )) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (!mmay_bytecode_nominal_consume(work, limit)) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        expected_method_start = method_end;
    }
    return expected_method_start == function->interface_methods.length
        ? MMAY_BYTECODE_VERIFY_OK
        : MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

MarimayBytecodeVerifyStatus mmay_bytecode_verify_nominal_metadata(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
) {
    size_t register_index = 0;
    MarimayBytecodeVerifyStatus status = MMAY_BYTECODE_VERIFY_OK;
    if (work_used == NULL) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    *work_used = 0;
    if (function == NULL || !function->initialized
        || function->register_layouts.length != function->register_types.length
        || function->record_layouts.length > function->options.max_record_layouts
        || function->record_fields.length > function->options.max_record_fields
        || function->record_constructions.length
            > function->options.max_record_constructions
        || function->record_arguments.length
            > function->options.max_record_arguments
        || function->enum_layouts.length > function->options.max_enum_layouts
        || function->enum_variants.length > function->options.max_enum_variants
        || function->enum_payloads.length > function->options.max_enum_payloads
        || function->enum_constructions.length
            > function->options.max_enum_constructions
        || function->enum_arguments.length
            > function->options.max_enum_arguments
        || function->interface_layouts.length
            > function->options.max_interface_layouts
        || function->interface_constructions.length
            > function->options.max_interface_constructions
        || function->interface_methods.length
            > function->options.max_interface_methods) {
        return MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (register_index = 0; register_index < function->register_types.length;
        register_index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            register_index
        );
        uint32_t layout = mmay_bytecode_register_layout(
            function,
            (uint32_t)register_index
        );
        if (type == NULL
            || !mmay_bytecode_type_layout_is_valid(function, *type, layout)) {
            return MMAY_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (!mmay_bytecode_nominal_consume(work_used, available_work)) {
            return MMAY_BYTECODE_VERIFY_LIMIT_REACHED;
        }
    }
    status = mmay_bytecode_verify_record_metadata(
        function,
        available_work,
        work_used
    );
    if (status == MMAY_BYTECODE_VERIFY_OK) {
        status = mmay_bytecode_verify_enum_metadata(
            function,
            available_work,
            work_used
        );
    }
    if (status == MMAY_BYTECODE_VERIFY_OK) {
        status = mmay_bytecode_verify_interface_metadata(
            function,
            available_work,
            work_used
        );
    }
    return status;
}
