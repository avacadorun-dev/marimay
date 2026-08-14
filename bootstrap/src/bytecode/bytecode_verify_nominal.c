#include "bytecode_internal.h"

#include <limits.h>

static bool ruda_bytecode_nominal_consume(
    size_t *used,
    size_t limit
) {
    size_t next = 0;
    if (used == NULL || !ruda_size_add(*used, 1, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_record_metadata(
    const RudaBytecodeFunction *function,
    size_t limit,
    size_t *work
) {
    size_t layout_index = 0;
    size_t expected_field_start = 0;
    size_t construction_index = 0;
    size_t expected_argument_start = 0;
    for (layout_index = 0; layout_index < function->record_layouts.length;
        layout_index += 1) {
        const RudaBytecodeRecordLayout *layout = ruda_bytecode_record_layout(
            function,
            (RudaBytecodeRecordId)layout_index
        );
        size_t field_index = 0;
        size_t field_end = 0;
        if (layout == NULL || layout->field_start != expected_field_start
            || !ruda_size_add(
                layout->field_start,
                layout->field_count,
                &field_end
            ) || field_end > function->record_fields.length) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_bytecode_nominal_consume(work, limit)) {
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (field_index = 0; field_index < layout->field_count;
            field_index += 1) {
            const RudaBytecodeRecordField *field = ruda_bytecode_record_field(
                function,
                layout,
                field_index
            );
            if (field == NULL || !ruda_bytecode_type_is_register(field->type)
                || !ruda_bytecode_type_layout_is_valid(
                    function,
                    field->type,
                    field->layout
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!ruda_bytecode_nominal_consume(work, limit)) {
                return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_field_start = field_end;
    }
    if (expected_field_start != function->record_fields.length) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (construction_index = 0;
        construction_index < function->record_constructions.length;
        construction_index += 1) {
        const RudaBytecodeRecordConstruction *construction =
            ruda_bytecode_record_construction(
                function,
                (RudaBytecodeRecordConstructionId)construction_index
            );
        const RudaBytecodeRecordLayout *layout = construction != NULL
            ? ruda_bytecode_record_layout(function, construction->record)
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL
            || construction->argument_start != expected_argument_start
            || construction->argument_count != layout->field_count
            || !ruda_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->record_arguments.length) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_bytecode_nominal_consume(work, limit)) {
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaBytecodeRecordArgument *argument = ruda_vector_at_const(
                &function->record_arguments,
                construction->argument_start + argument_index
            );
            const RudaBytecodeRecordField *field = ruda_bytecode_record_field(
                function,
                layout,
                argument_index
            );
            const RudaBytecodeType *actual = argument != NULL
                    && argument->register_id < function->register_types.length
                ? ruda_vector_at_const(
                    &function->register_types,
                    argument->register_id
                )
                : NULL;
            if (argument == NULL || actual == NULL) {
                return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            if (field == NULL || argument->type != field->type
                || argument->layout != field->layout
                || *actual != argument->type
                || ruda_bytecode_register_layout(
                    function,
                    argument->register_id
                ) != argument->layout) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!ruda_bytecode_nominal_consume(work, limit)) {
                return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_argument_start = argument_end;
    }
    return expected_argument_start == function->record_arguments.length
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_enum_metadata(
    const RudaBytecodeFunction *function,
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
        const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
            function,
            (RudaBytecodeEnumId)layout_index
        );
        size_t variant_end = 0;
        size_t payload_end = 0;
        size_t variant_index = 0;
        size_t payload_index = 0;
        size_t local_payload_start = 0;
        if (layout == NULL
            || layout->variant_start != expected_variant_start
            || layout->payload_start != expected_payload_start
            || !ruda_size_add(
                layout->variant_start,
                layout->variant_count,
                &variant_end
            ) || variant_end > function->enum_variants.length
            || !ruda_size_add(
                layout->payload_start,
                layout->payload_count,
                &payload_end
            ) || payload_end > function->enum_payloads.length) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_bytecode_nominal_consume(work, limit)) {
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (variant_index = 0; variant_index < layout->variant_count;
            variant_index += 1) {
            const RudaBytecodeEnumVariant *variant = ruda_vector_at_const(
                &function->enum_variants,
                layout->variant_start + variant_index
            );
            size_t local_payload_end = 0;
            if (variant == NULL
                || variant->payload_start != local_payload_start
                || !ruda_size_add(
                    variant->payload_start,
                    variant->payload_count,
                    &local_payload_end
                ) || local_payload_end > layout->payload_count) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            local_payload_start = local_payload_end;
            if (!ruda_bytecode_nominal_consume(work, limit)) {
                return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        if (local_payload_start != layout->payload_count) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        for (payload_index = 0; payload_index < layout->payload_count;
            payload_index += 1) {
            const RudaBytecodeEnumPayload *payload = ruda_vector_at_const(
                &function->enum_payloads,
                layout->payload_start + payload_index
            );
            if (payload == NULL
                || !ruda_bytecode_type_is_register(payload->type)
                || !ruda_bytecode_type_layout_is_valid(
                    function,
                    payload->type,
                    payload->layout
                )) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!ruda_bytecode_nominal_consume(work, limit)) {
                return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_variant_start = variant_end;
        expected_payload_start = payload_end;
    }
    if (expected_variant_start != function->enum_variants.length
        || expected_payload_start != function->enum_payloads.length) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (construction_index = 0;
        construction_index < function->enum_constructions.length;
        construction_index += 1) {
        const RudaBytecodeEnumConstruction *construction =
            ruda_bytecode_enum_construction(
                function,
                (RudaBytecodeEnumConstructionId)construction_index
            );
        const RudaBytecodeEnumLayout *layout = construction != NULL
            ? ruda_bytecode_enum_layout(function, construction->enum_id)
            : NULL;
        const RudaBytecodeEnumVariant *variant = layout != NULL
                && construction->variant < layout->variant_count
            ? ruda_vector_at_const(
                &function->enum_variants,
                layout->variant_start + construction->variant
            )
            : NULL;
        size_t argument_end = 0;
        size_t argument_index = 0;
        if (construction == NULL || layout == NULL || variant == NULL
            || construction->argument_start != expected_argument_start
            || construction->argument_count != variant->payload_count
            || !ruda_size_add(
                construction->argument_start,
                construction->argument_count,
                &argument_end
            ) || argument_end > function->enum_arguments.length) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_bytecode_nominal_consume(work, limit)) {
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaBytecodeEnumArgument *argument = ruda_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + argument_index
            );
            const RudaBytecodeEnumPayload *payload = ruda_vector_at_const(
                &function->enum_payloads,
                layout->payload_start + variant->payload_start + argument_index
            );
            const RudaBytecodeType *actual = argument != NULL
                    && argument->register_id < function->register_types.length
                ? ruda_vector_at_const(
                    &function->register_types,
                    argument->register_id
                )
                : NULL;
            if (argument == NULL || actual == NULL) {
                return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
            }
            if (payload == NULL || argument->type != payload->type
                || argument->layout != payload->layout
                || *actual != argument->type
                || ruda_bytecode_register_layout(
                    function,
                    argument->register_id
                ) != argument->layout) {
                return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            if (!ruda_bytecode_nominal_consume(work, limit)) {
                return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            }
        }
        expected_argument_start = argument_end;
    }
    return expected_argument_start == function->enum_arguments.length
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_interface_metadata(
    const RudaBytecodeFunction *function,
    size_t limit,
    size_t *work
) {
    size_t layout_index = 0;
    size_t construction_index = 0;
    size_t expected_method_start = 0;
    for (layout_index = 0; layout_index < function->interface_layouts.length;
        layout_index += 1) {
        const RudaBytecodeInterfaceLayout *layout =
            ruda_bytecode_interface_layout(
                function,
                (RudaBytecodeInterfaceId)layout_index
            );
        if (layout == NULL || !ruda_bytecode_nominal_consume(work, limit)) {
            return layout == NULL
                ? RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT
                : RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
    }
    for (construction_index = 0;
        construction_index < function->interface_constructions.length;
        construction_index += 1) {
        const RudaBytecodeInterfaceConstruction *construction =
            ruda_bytecode_interface_construction(
                function,
                (RudaBytecodeInterfaceConstructionId)construction_index
            );
        const RudaBytecodeInterfaceLayout *layout = construction != NULL
            ? ruda_bytecode_interface_layout(
                function,
                construction->interface_id
            )
            : NULL;
        const RudaBytecodeType *actual = construction != NULL
                && construction->payload_register
                    < function->register_types.length
            ? ruda_vector_at_const(
                &function->register_types,
                construction->payload_register
            )
            : NULL;
        size_t method_end = 0;
        if (construction == NULL || layout == NULL || actual == NULL
            || construction->method_start != expected_method_start
            || construction->method_count != layout->method_count
            || !ruda_size_add(
                construction->method_start,
                construction->method_count,
                &method_end
            ) || method_end > function->interface_methods.length) {
            return actual == NULL
                ? RUDA_BYTECODE_VERIFY_INVALID_REGISTER
                : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
        if (*actual != construction->payload_type
            || ruda_bytecode_register_layout(
                function,
                construction->payload_register
            ) != construction->payload_layout
            || !ruda_bytecode_type_layout_is_valid(
                function,
                construction->payload_type,
                construction->payload_layout
            )) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (!ruda_bytecode_nominal_consume(work, limit)) {
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
        expected_method_start = method_end;
    }
    return expected_method_start == function->interface_methods.length
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify_nominal_metadata(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
) {
    size_t register_index = 0;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (work_used == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
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
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    for (register_index = 0; register_index < function->register_types.length;
        register_index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            register_index
        );
        uint32_t layout = ruda_bytecode_register_layout(
            function,
            (uint32_t)register_index
        );
        if (type == NULL
            || !ruda_bytecode_type_layout_is_valid(function, *type, layout)) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        if (!ruda_bytecode_nominal_consume(work_used, available_work)) {
            return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        }
    }
    status = ruda_bytecode_verify_record_metadata(
        function,
        available_work,
        work_used
    );
    if (status == RUDA_BYTECODE_VERIFY_OK) {
        status = ruda_bytecode_verify_enum_metadata(
            function,
            available_work,
            work_used
        );
    }
    if (status == RUDA_BYTECODE_VERIFY_OK) {
        status = ruda_bytecode_verify_interface_metadata(
            function,
            available_work,
            work_used
        );
    }
    return status;
}
