#include "bytecode_codec_internal.h"
#include "bytecode_codec_nominal_values.h"
#include "bytecode_internal.h"

#include <limits.h>
#include <string.h>

static RudaBytecodeCodecStatus ruda_bytecode_nominal_write_status(
    const RudaBytecodeEncodeBuffer *buffer
) {
    return buffer != NULL && buffer->limit_reached
        ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
        : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
}

static bool ruda_bytecode_nominal_type(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_RECORD
        || type == RUDA_BYTECODE_TYPE_ENUM
        || type == RUDA_BYTECODE_TYPE_INTERFACE;
}

bool ruda_bytecode_function_uses_nominal(
    const RudaBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL) {
        return false;
    }
    if (ruda_bytecode_nominal_type(function->result_type)
        || function->record_layouts.length != 0
        || function->record_constructions.length != 0
        || function->enum_layouts.length != 0
        || function->enum_constructions.length != 0
        || function->interface_layouts.length != 0
        || function->interface_constructions.length != 0) {
        return true;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && ruda_bytecode_nominal_type(*type)) {
            return true;
        }
    }
    for (index = 0; index < function->calls.length; index += 1) {
        const RudaBytecodeCall *call = ruda_vector_at_const(
            &function->calls,
            index
        );
        if (call != NULL && (ruda_bytecode_nominal_type(call->result_type)
                || call->interface_slot != UINT32_MAX)) {
            return true;
        }
    }
    for (index = 0; index < function->call_arguments.length; index += 1) {
        const RudaBytecodeCallArgument *argument = ruda_vector_at_const(
            &function->call_arguments,
            index
        );
        if (argument != NULL && ruda_bytecode_nominal_type(argument->type)) {
            return true;
        }
    }
    return false;
}

static bool ruda_bytecode_nominal_encode_type_layout(
    RudaBytecodeEncodeBuffer *buffer,
    RudaBytecodeType type,
    uint32_t layout
) {
    return ruda_bytecode_codec_encode_u64(buffer, (uint64_t)type)
        && ruda_bytecode_codec_encode_u64(buffer, layout);
}

static bool ruda_bytecode_nominal_encode_symbol(
    RudaBytecodeEncodeBuffer *buffer,
    RudaSymbolId symbol
) {
    return ruda_bytecode_codec_encode_append(
        buffer,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
}

static bool ruda_bytecode_nominal_encode_layouts(
    const RudaBytecodeFunction *function,
    RudaBytecodeEncodeBuffer *buffer
) {
    size_t index = 0;
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->record_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->record_layouts.length; index += 1) {
        const RudaBytecodeRecordLayout *layout = ruda_bytecode_record_layout(
            function,
            (RudaBytecodeRecordId)index
        );
        size_t field_index = 0;
        if (layout == NULL || !ruda_bytecode_nominal_encode_symbol(
                buffer,
                layout->symbol
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->field_count
            )) {
            return false;
        }
        for (field_index = 0; field_index < layout->field_count;
            field_index += 1) {
            const RudaBytecodeRecordField *field = ruda_bytecode_record_field(
                function,
                layout,
                field_index
            );
            if (field == NULL || !ruda_bytecode_nominal_encode_type_layout(
                    buffer,
                    field->type,
                    field->layout
                ) || !ruda_bytecode_codec_encode_u64(
                    buffer,
                    field->is_mutable ? 1U : 0U
                )) {
                return false;
            }
        }
    }
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->enum_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->enum_layouts.length; index += 1) {
        const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
            function,
            (RudaBytecodeEnumId)index
        );
        size_t variant_index = 0;
        size_t payload_index = 0;
        if (layout == NULL || !ruda_bytecode_nominal_encode_symbol(
                buffer,
                layout->symbol
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->variant_count
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->payload_count
            )) {
            return false;
        }
        for (variant_index = 0; variant_index < layout->variant_count;
            variant_index += 1) {
            const RudaBytecodeEnumVariant *variant = ruda_vector_at_const(
                &function->enum_variants,
                layout->variant_start + variant_index
            );
            if (variant == NULL || !ruda_bytecode_codec_encode_u64(
                    buffer,
                    (uint64_t)variant->payload_start
                ) || !ruda_bytecode_codec_encode_u64(
                    buffer,
                    (uint64_t)variant->payload_count
                )) {
                return false;
            }
        }
        for (payload_index = 0; payload_index < layout->payload_count;
            payload_index += 1) {
            const RudaBytecodeEnumPayload *payload = ruda_vector_at_const(
                &function->enum_payloads,
                layout->payload_start + payload_index
            );
            if (payload == NULL || !ruda_bytecode_nominal_encode_type_layout(
                    buffer,
                    payload->type,
                    payload->layout
                )) {
                return false;
            }
        }
    }
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->interface_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->interface_layouts.length; index += 1) {
        const RudaBytecodeInterfaceLayout *layout =
            ruda_bytecode_interface_layout(
                function,
                (RudaBytecodeInterfaceId)index
            );
        if (layout == NULL || !ruda_bytecode_nominal_encode_symbol(
                buffer,
                layout->symbol
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->method_count
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_bytecode_nominal_encode_constructions(
    const RudaBytecodeFunction *function,
    RudaBytecodeEncodeBuffer *buffer
) {
    size_t index = 0;
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->record_constructions.length
        )) {
        return false;
    }
    for (index = 0; index < function->record_constructions.length;
        index += 1) {
        const RudaBytecodeRecordConstruction *construction =
            ruda_bytecode_record_construction(
                function,
                (RudaBytecodeRecordConstructionId)index
            );
        size_t argument_index = 0;
        if (construction == NULL || !ruda_bytecode_codec_encode_u64(
                buffer,
                construction->record
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->argument_count
            )) {
            return false;
        }
        for (argument_index = 0; argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaBytecodeRecordArgument *argument = ruda_vector_at_const(
                &function->record_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !ruda_bytecode_codec_encode_u64(
                    buffer,
                    argument->register_id
                ) || !ruda_bytecode_nominal_encode_type_layout(
                    buffer,
                    argument->type,
                    argument->layout
                )) {
                return false;
            }
        }
    }
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->enum_constructions.length
        )) {
        return false;
    }
    for (index = 0; index < function->enum_constructions.length; index += 1) {
        const RudaBytecodeEnumConstruction *construction =
            ruda_bytecode_enum_construction(
                function,
                (RudaBytecodeEnumConstructionId)index
            );
        size_t argument_index = 0;
        if (construction == NULL || !ruda_bytecode_codec_encode_u64(
                buffer,
                construction->enum_id
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->variant
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->argument_count
            )) {
            return false;
        }
        for (argument_index = 0; argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaBytecodeEnumArgument *argument = ruda_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !ruda_bytecode_codec_encode_u64(
                    buffer,
                    argument->register_id
                ) || !ruda_bytecode_nominal_encode_type_layout(
                    buffer,
                    argument->type,
                    argument->layout
                )) {
                return false;
            }
        }
    }
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->interface_constructions.length
        )) {
        return false;
    }
    for (index = 0; index < function->interface_constructions.length;
        index += 1) {
        const RudaBytecodeInterfaceConstruction *construction =
            ruda_bytecode_interface_construction(
                function,
                (RudaBytecodeInterfaceConstructionId)index
            );
        size_t method_index = 0;
        if (construction == NULL || !ruda_bytecode_codec_encode_u64(
                buffer,
                construction->interface_id
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                construction->payload_register
            ) || !ruda_bytecode_nominal_encode_type_layout(
                buffer,
                construction->payload_type,
                construction->payload_layout
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->method_count
            )) {
            return false;
        }
        for (method_index = 0; method_index < construction->method_count;
            method_index += 1) {
            const RudaSymbolId *method = ruda_vector_at_const(
                &function->interface_methods,
                construction->method_start + method_index
            );
            if (method == NULL || !ruda_bytecode_nominal_encode_symbol(
                    buffer,
                    *method
                )) {
                return false;
            }
        }
    }
    return true;
}

RudaBytecodeCodecStatus ruda_bytecode_encode_nominal_metadata(
    const RudaBytecodeFunction *function,
    RudaBytecodeEncodeBuffer *buffer
) {
    if (function == NULL || buffer == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (!ruda_bytecode_nominal_encode_layouts(function, buffer)
        || !ruda_bytecode_nominal_values_encode(function, buffer)
        || !ruda_bytecode_nominal_encode_constructions(function, buffer)) {
        return ruda_bytecode_nominal_write_status(buffer);
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_count(
    RudaBytecodeCursor *cursor,
    size_t limit,
    size_t *result
) {
    uint64_t value = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (!ruda_bytecode_codec_u64_to_size(value, result) || *result > limit) {
        return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_u32(
    RudaBytecodeCursor *cursor,
    uint32_t *result
) {
    uint64_t value = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value > UINT32_MAX) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    *result = (uint32_t)value;
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_symbol(
    RudaBytecodeCursor *cursor,
    RudaSymbolId *result
) {
    if (cursor == NULL || result == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (cursor->offset > cursor->length
        || cursor->length - cursor->offset < sizeof(result->bytes)) {
        return RUDA_BYTECODE_CODEC_TRUNCATED;
    }
    memcpy(result->bytes, cursor->bytes + cursor->offset, sizeof(result->bytes));
    cursor->offset += sizeof(result->bytes);
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_type_layout(
    RudaBytecodeCursor *cursor,
    RudaBytecodeType *type,
    uint32_t *layout
) {
    uint64_t value = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value > RUDA_BYTECODE_TYPE_INTERFACE
        || !ruda_bytecode_type_is_register((RudaBytecodeType)value)) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    *type = (RudaBytecodeType)value;
    return ruda_bytecode_nominal_decode_u32(cursor, layout);
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_record_layouts(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_count(
        cursor,
        function->options.max_record_layouts,
        &count
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        RudaSymbolId symbol;
        RudaBytecodeRecordField *fields = NULL;
        RudaBytecodeRecordId id = RUDA_BYTECODE_NO_RECORD;
        size_t field_count = 0;
        size_t bytes = 0;
        size_t field_index = 0;
        status = ruda_bytecode_nominal_decode_symbol(cursor, &symbol);
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_record_fields
                    - function->record_fields.length,
                &field_count
            );
        }
        if (status != RUDA_BYTECODE_CODEC_OK
            || !ruda_size_multiply(field_count, sizeof(*fields), &bytes)) {
            return status != RUDA_BYTECODE_CODEC_OK
                ? status
                : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            fields = ruda_allocate(&function->allocator, bytes);
            if (fields == NULL) {
                return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
            memset(fields, 0, bytes);
        }
        for (field_index = 0; field_index < field_count; field_index += 1) {
            uint64_t mutable_wire = 0;
            status = ruda_bytecode_nominal_decode_type_layout(
                cursor,
                &fields[field_index].type,
                &fields[field_index].layout
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                status = ruda_bytecode_codec_decode_u64(cursor, &mutable_wire);
            }
            if (status != RUDA_BYTECODE_CODEC_OK || mutable_wire > 1) {
                status = status != RUDA_BYTECODE_CODEC_OK
                    ? status
                    : RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                break;
            }
            fields[field_index].is_mutable = mutable_wire != 0;
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && !ruda_bytecode_add_record_layout(
                function,
                symbol,
                fields,
                field_count,
                &id
            )) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        ruda_deallocate(&function->allocator, fields, bytes);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_enum_layouts(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_count(
        cursor,
        function->options.max_enum_layouts,
        &count
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        RudaSymbolId symbol;
        RudaBytecodeEnumVariant *variants = NULL;
        RudaBytecodeEnumPayload *payloads = NULL;
        RudaBytecodeEnumId id = RUDA_BYTECODE_NO_ENUM;
        size_t variant_count = 0;
        size_t payload_count = 0;
        size_t variant_bytes = 0;
        size_t payload_bytes = 0;
        size_t item = 0;
        status = ruda_bytecode_nominal_decode_symbol(cursor, &symbol);
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_enum_variants
                    - function->enum_variants.length,
                &variant_count
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_enum_payloads
                    - function->enum_payloads.length,
                &payload_count
            );
        }
        if (status != RUDA_BYTECODE_CODEC_OK
            || !ruda_size_multiply(
                variant_count,
                sizeof(*variants),
                &variant_bytes
            ) || !ruda_size_multiply(
                payload_count,
                sizeof(*payloads),
                &payload_bytes
            )) {
            return status != RUDA_BYTECODE_CODEC_OK
                ? status
                : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (variant_bytes != 0) {
            variants = ruda_allocate(&function->allocator, variant_bytes);
        }
        if (payload_bytes != 0) {
            payloads = ruda_allocate(&function->allocator, payload_bytes);
        }
        if ((variant_bytes != 0 && variants == NULL)
            || (payload_bytes != 0 && payloads == NULL)) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        for (item = 0; status == RUDA_BYTECODE_CODEC_OK && item < variant_count;
            item += 1) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                payload_count,
                &variants[item].payload_start
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                status = ruda_bytecode_nominal_decode_count(
                    cursor,
                    payload_count,
                    &variants[item].payload_count
                );
            }
            if (status == RUDA_BYTECODE_CODEC_OK
                && (variants[item].payload_start > payload_count
                    || variants[item].payload_count
                        > payload_count - variants[item].payload_start)) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
            }
        }
        for (item = 0; status == RUDA_BYTECODE_CODEC_OK && item < payload_count;
            item += 1) {
            status = ruda_bytecode_nominal_decode_type_layout(
                cursor,
                &payloads[item].type,
                &payloads[item].layout
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && !ruda_bytecode_add_enum_layout(
                function,
                symbol,
                variants,
                variant_count,
                payloads,
                payload_count,
                &id
            )) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        ruda_deallocate(&function->allocator, payloads, payload_bytes);
        ruda_deallocate(&function->allocator, variants, variant_bytes);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_interface_layouts(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_count(
        cursor,
        function->options.max_interface_layouts,
        &count
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        RudaSymbolId symbol;
        RudaBytecodeInterfaceId id = RUDA_BYTECODE_NO_INTERFACE;
        size_t method_count = 0;
        status = ruda_bytecode_nominal_decode_symbol(cursor, &symbol);
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_interface_methods,
                &method_count
            );
        }
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!ruda_bytecode_add_interface_layout(
                function,
                symbol,
                method_count,
                &id
            )) {
            return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if ((size_t)id != index) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_argument(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor,
    uint32_t *register_id,
    RudaBytecodeType *type,
    uint32_t *layout
) {
    const RudaBytecodeType *actual = NULL;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_u32(
        cursor,
        register_id
    );
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_decode_type_layout(cursor, type, layout);
    }
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    actual = *register_id < function->register_types.length
        ? ruda_vector_at_const(&function->register_types, *register_id)
        : NULL;
    if (actual == NULL || *actual != *type
        || ruda_bytecode_register_layout(function, *register_id) != *layout) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_record_constructions(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_count(
        cursor,
        function->options.max_record_constructions,
        &count
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        uint32_t record = RUDA_BYTECODE_NO_RECORD;
        RudaBytecodeRecordArgument *arguments = NULL;
        RudaBytecodeRecordConstructionId id =
            RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
        const RudaBytecodeRecordLayout *record_layout = NULL;
        size_t argument_count = 0;
        size_t bytes = 0;
        size_t item = 0;
        status = ruda_bytecode_nominal_decode_u32(cursor, &record);
        record_layout = status == RUDA_BYTECODE_CODEC_OK
            ? ruda_bytecode_record_layout(function, record)
            : NULL;
        if (status == RUDA_BYTECODE_CODEC_OK && record_layout == NULL) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_record_arguments
                    - function->record_arguments.length,
                &argument_count
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && argument_count != record_layout->field_count) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status != RUDA_BYTECODE_CODEC_OK
            || !ruda_size_multiply(argument_count, sizeof(*arguments), &bytes)) {
            return status != RUDA_BYTECODE_CODEC_OK
                ? status
                : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            arguments = ruda_allocate(&function->allocator, bytes);
            if (arguments == NULL) {
                return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
        }
        for (item = 0; item < argument_count; item += 1) {
            status = ruda_bytecode_nominal_decode_argument(
                function,
                cursor,
                &arguments[item].register_id,
                &arguments[item].type,
                &arguments[item].layout
            );
            if (status != RUDA_BYTECODE_CODEC_OK) {
                break;
            }
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && (!ruda_vector_reserve(
                    &function->record_constructions,
                    function->record_constructions.length + 1
                ) || !ruda_vector_reserve(
                    &function->record_arguments,
                    function->record_arguments.length + argument_count
                ))) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && !ruda_bytecode_add_record_construction(
                function,
                record,
                arguments,
                argument_count,
                &id
            )) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        ruda_deallocate(&function->allocator, arguments, bytes);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_enum_constructions(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_count(
        cursor,
        function->options.max_enum_constructions,
        &count
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        uint32_t enum_id = RUDA_BYTECODE_NO_ENUM;
        RudaBytecodeEnumArgument *arguments = NULL;
        RudaBytecodeEnumConstructionId id = RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
        const RudaBytecodeEnumLayout *layout = NULL;
        size_t variant = 0;
        size_t argument_count = 0;
        size_t bytes = 0;
        size_t item = 0;
        status = ruda_bytecode_nominal_decode_u32(cursor, &enum_id);
        layout = status == RUDA_BYTECODE_CODEC_OK
            ? ruda_bytecode_enum_layout(function, enum_id)
            : NULL;
        if (status == RUDA_BYTECODE_CODEC_OK && layout == NULL) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                layout->variant_count,
                &variant
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK && variant >= layout->variant_count) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_enum_arguments
                    - function->enum_arguments.length,
                &argument_count
            );
        }
        if (status != RUDA_BYTECODE_CODEC_OK
            || !ruda_size_multiply(argument_count, sizeof(*arguments), &bytes)) {
            return status != RUDA_BYTECODE_CODEC_OK
                ? status
                : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            arguments = ruda_allocate(&function->allocator, bytes);
            if (arguments == NULL) {
                return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
        }
        for (item = 0; item < argument_count; item += 1) {
            status = ruda_bytecode_nominal_decode_argument(
                function,
                cursor,
                &arguments[item].register_id,
                &arguments[item].type,
                &arguments[item].layout
            );
            if (status != RUDA_BYTECODE_CODEC_OK) {
                break;
            }
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && (!ruda_vector_reserve(
                    &function->enum_constructions,
                    function->enum_constructions.length + 1
                ) || !ruda_vector_reserve(
                    &function->enum_arguments,
                    function->enum_arguments.length + argument_count
                ))) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && !ruda_bytecode_add_enum_construction(
                function,
                enum_id,
                variant,
                arguments,
                argument_count,
                &id
            )) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        ruda_deallocate(&function->allocator, arguments, bytes);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_decode_interface_constructions(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_decode_count(
        cursor,
        function->options.max_interface_constructions,
        &count
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        uint32_t interface_id = RUDA_BYTECODE_NO_INTERFACE;
        uint32_t payload_register = UINT32_MAX;
        RudaBytecodeType payload_type = RUDA_BYTECODE_TYPE_INVALID;
        uint32_t payload_layout = RUDA_BYTECODE_NO_LAYOUT;
        RudaSymbolId *methods = NULL;
        RudaBytecodeInterfaceConstructionId id =
            RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
        const RudaBytecodeInterfaceLayout *layout = NULL;
        size_t method_count = 0;
        size_t bytes = 0;
        size_t item = 0;
        status = ruda_bytecode_nominal_decode_u32(cursor, &interface_id);
        layout = status == RUDA_BYTECODE_CODEC_OK
            ? ruda_bytecode_interface_layout(function, interface_id)
            : NULL;
        if (status == RUDA_BYTECODE_CODEC_OK && layout == NULL) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_u32(
                cursor,
                &payload_register
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_type_layout(
                cursor,
                &payload_type,
                &payload_layout
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_decode_count(
                cursor,
                function->options.max_interface_methods
                    - function->interface_methods.length,
                &method_count
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && method_count != layout->method_count) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status != RUDA_BYTECODE_CODEC_OK
            || !ruda_size_multiply(method_count, sizeof(*methods), &bytes)) {
            return status != RUDA_BYTECODE_CODEC_OK
                ? status
                : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            methods = ruda_allocate(&function->allocator, bytes);
            if (methods == NULL) {
                return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
        }
        for (item = 0; item < method_count; item += 1) {
            status = ruda_bytecode_nominal_decode_symbol(cursor, &methods[item]);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                break;
            }
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && (!ruda_vector_reserve(
                    &function->interface_constructions,
                    function->interface_constructions.length + 1
                ) || !ruda_vector_reserve(
                    &function->interface_methods,
                    function->interface_methods.length + method_count
                ))) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if (status == RUDA_BYTECODE_CODEC_OK
            && !ruda_bytecode_add_interface_construction(
                function,
                interface_id,
                payload_register,
                payload_type,
                payload_layout,
                methods,
                method_count,
                &id
            )) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        ruda_deallocate(&function->allocator, methods, bytes);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}

RudaBytecodeCodecStatus ruda_bytecode_decode_nominal_metadata(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    RudaBytecodeCodecStatus status = RUDA_BYTECODE_CODEC_OK;
    if (function == NULL || cursor == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    status = ruda_bytecode_nominal_decode_record_layouts(function, cursor);
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_decode_enum_layouts(function, cursor);
    }
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_decode_interface_layouts(function, cursor);
    }
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_values_decode(function, cursor);
    }
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_decode_record_constructions(
            function,
            cursor
        );
    }
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_decode_enum_constructions(function, cursor);
    }
    if (status == RUDA_BYTECODE_CODEC_OK) {
        status = ruda_bytecode_nominal_decode_interface_constructions(
            function,
            cursor
        );
    }
    return status;
}
