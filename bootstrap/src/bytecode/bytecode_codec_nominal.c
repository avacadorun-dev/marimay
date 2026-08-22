#include "bytecode_codec_internal.h"
#include "bytecode_codec_nominal_values.h"
#include "bytecode_internal.h"

#include <limits.h>
#include <string.h>

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_write_status(
    const MarimayBytecodeEncodeBuffer *buffer
) {
    return buffer != NULL && buffer->limit_reached
        ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
        : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
}

static bool mmay_bytecode_nominal_type(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_RECORD
        || type == MMAY_BYTECODE_TYPE_ENUM
        || type == MMAY_BYTECODE_TYPE_INTERFACE;
}

bool mmay_bytecode_function_uses_nominal(
    const MarimayBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL) {
        return false;
    }
    if (mmay_bytecode_nominal_type(function->result_type)
        || function->record_layouts.length != 0
        || function->record_constructions.length != 0
        || function->enum_layouts.length != 0
        || function->enum_constructions.length != 0
        || function->interface_layouts.length != 0
        || function->interface_constructions.length != 0) {
        return true;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && mmay_bytecode_nominal_type(*type)) {
            return true;
        }
    }
    for (index = 0; index < function->calls.length; index += 1) {
        const MarimayBytecodeCall *call = mmay_vector_at_const(
            &function->calls,
            index
        );
        if (call != NULL && (mmay_bytecode_nominal_type(call->result_type)
                || call->interface_slot != UINT32_MAX)) {
            return true;
        }
    }
    for (index = 0; index < function->call_arguments.length; index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_vector_at_const(
            &function->call_arguments,
            index
        );
        if (argument != NULL && mmay_bytecode_nominal_type(argument->type)) {
            return true;
        }
    }
    return false;
}

static bool mmay_bytecode_nominal_encode_type_layout(
    MarimayBytecodeEncodeBuffer *buffer,
    MarimayBytecodeType type,
    uint32_t layout
) {
    return mmay_bytecode_codec_encode_u64(buffer, (uint64_t)type)
        && mmay_bytecode_codec_encode_u64(buffer, layout);
}

static bool mmay_bytecode_nominal_encode_symbol(
    MarimayBytecodeEncodeBuffer *buffer,
    MarimaySymbolId symbol
) {
    return mmay_bytecode_codec_encode_append(
        buffer,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
}

static bool mmay_bytecode_nominal_encode_layouts(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEncodeBuffer *buffer
) {
    size_t index = 0;
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->record_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->record_layouts.length; index += 1) {
        const MarimayBytecodeRecordLayout *layout = mmay_bytecode_record_layout(
            function,
            (MarimayBytecodeRecordId)index
        );
        size_t field_index = 0;
        if (layout == NULL || !mmay_bytecode_nominal_encode_symbol(
                buffer,
                layout->symbol
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->field_count
            )) {
            return false;
        }
        for (field_index = 0; field_index < layout->field_count;
            field_index += 1) {
            const MarimayBytecodeRecordField *field = mmay_bytecode_record_field(
                function,
                layout,
                field_index
            );
            if (field == NULL || !mmay_bytecode_nominal_encode_type_layout(
                    buffer,
                    field->type,
                    field->layout
                ) || !mmay_bytecode_codec_encode_u64(
                    buffer,
                    field->is_mutable ? 1U : 0U
                )) {
                return false;
            }
        }
    }
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->enum_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->enum_layouts.length; index += 1) {
        const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
            function,
            (MarimayBytecodeEnumId)index
        );
        size_t variant_index = 0;
        size_t payload_index = 0;
        if (layout == NULL || !mmay_bytecode_nominal_encode_symbol(
                buffer,
                layout->symbol
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->variant_count
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->payload_count
            )) {
            return false;
        }
        for (variant_index = 0; variant_index < layout->variant_count;
            variant_index += 1) {
            const MarimayBytecodeEnumVariant *variant = mmay_vector_at_const(
                &function->enum_variants,
                layout->variant_start + variant_index
            );
            if (variant == NULL || !mmay_bytecode_codec_encode_u64(
                    buffer,
                    (uint64_t)variant->payload_start
                ) || !mmay_bytecode_codec_encode_u64(
                    buffer,
                    (uint64_t)variant->payload_count
                )) {
                return false;
            }
        }
        for (payload_index = 0; payload_index < layout->payload_count;
            payload_index += 1) {
            const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
                &function->enum_payloads,
                layout->payload_start + payload_index
            );
            if (payload == NULL || !mmay_bytecode_nominal_encode_type_layout(
                    buffer,
                    payload->type,
                    payload->layout
                )) {
                return false;
            }
        }
    }
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->interface_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->interface_layouts.length; index += 1) {
        const MarimayBytecodeInterfaceLayout *layout =
            mmay_bytecode_interface_layout(
                function,
                (MarimayBytecodeInterfaceId)index
            );
        if (layout == NULL || !mmay_bytecode_nominal_encode_symbol(
                buffer,
                layout->symbol
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)layout->method_count
            )) {
            return false;
        }
    }
    return true;
}

static bool mmay_bytecode_nominal_encode_constructions(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEncodeBuffer *buffer
) {
    size_t index = 0;
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->record_constructions.length
        )) {
        return false;
    }
    for (index = 0; index < function->record_constructions.length;
        index += 1) {
        const MarimayBytecodeRecordConstruction *construction =
            mmay_bytecode_record_construction(
                function,
                (MarimayBytecodeRecordConstructionId)index
            );
        size_t argument_index = 0;
        if (construction == NULL || !mmay_bytecode_codec_encode_u64(
                buffer,
                construction->record
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->argument_count
            )) {
            return false;
        }
        for (argument_index = 0; argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayBytecodeRecordArgument *argument = mmay_vector_at_const(
                &function->record_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !mmay_bytecode_codec_encode_u64(
                    buffer,
                    argument->register_id
                ) || !mmay_bytecode_nominal_encode_type_layout(
                    buffer,
                    argument->type,
                    argument->layout
                )) {
                return false;
            }
        }
    }
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->enum_constructions.length
        )) {
        return false;
    }
    for (index = 0; index < function->enum_constructions.length; index += 1) {
        const MarimayBytecodeEnumConstruction *construction =
            mmay_bytecode_enum_construction(
                function,
                (MarimayBytecodeEnumConstructionId)index
            );
        size_t argument_index = 0;
        if (construction == NULL || !mmay_bytecode_codec_encode_u64(
                buffer,
                construction->enum_id
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->variant
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->argument_count
            )) {
            return false;
        }
        for (argument_index = 0; argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayBytecodeEnumArgument *argument = mmay_vector_at_const(
                &function->enum_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !mmay_bytecode_codec_encode_u64(
                    buffer,
                    argument->register_id
                ) || !mmay_bytecode_nominal_encode_type_layout(
                    buffer,
                    argument->type,
                    argument->layout
                )) {
                return false;
            }
        }
    }
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->interface_constructions.length
        )) {
        return false;
    }
    for (index = 0; index < function->interface_constructions.length;
        index += 1) {
        const MarimayBytecodeInterfaceConstruction *construction =
            mmay_bytecode_interface_construction(
                function,
                (MarimayBytecodeInterfaceConstructionId)index
            );
        size_t method_index = 0;
        if (construction == NULL || !mmay_bytecode_codec_encode_u64(
                buffer,
                construction->interface_id
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                construction->payload_register
            ) || !mmay_bytecode_nominal_encode_type_layout(
                buffer,
                construction->payload_type,
                construction->payload_layout
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)construction->method_count
            )) {
            return false;
        }
        for (method_index = 0; method_index < construction->method_count;
            method_index += 1) {
            const MarimaySymbolId *method = mmay_vector_at_const(
                &function->interface_methods,
                construction->method_start + method_index
            );
            if (method == NULL || !mmay_bytecode_nominal_encode_symbol(
                    buffer,
                    *method
                )) {
                return false;
            }
        }
    }
    return true;
}

MarimayBytecodeCodecStatus mmay_bytecode_encode_nominal_metadata(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEncodeBuffer *buffer
) {
    if (function == NULL || buffer == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (!mmay_bytecode_nominal_encode_layouts(function, buffer)
        || !mmay_bytecode_nominal_values_encode(function, buffer)
        || !mmay_bytecode_nominal_encode_constructions(function, buffer)) {
        return mmay_bytecode_nominal_write_status(buffer);
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_count(
    MarimayBytecodeCursor *cursor,
    size_t limit,
    size_t *result
) {
    uint64_t value = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (!mmay_bytecode_codec_u64_to_size(value, result) || *result > limit) {
        return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_u32(
    MarimayBytecodeCursor *cursor,
    uint32_t *result
) {
    uint64_t value = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value > UINT32_MAX) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    *result = (uint32_t)value;
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_symbol(
    MarimayBytecodeCursor *cursor,
    MarimaySymbolId *result
) {
    if (cursor == NULL || result == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (cursor->offset > cursor->length
        || cursor->length - cursor->offset < sizeof(result->bytes)) {
        return MMAY_BYTECODE_CODEC_TRUNCATED;
    }
    memcpy(result->bytes, cursor->bytes + cursor->offset, sizeof(result->bytes));
    cursor->offset += sizeof(result->bytes);
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_type_layout(
    MarimayBytecodeCursor *cursor,
    MarimayBytecodeType *type,
    uint32_t *layout
) {
    uint64_t value = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if ((value > MMAY_BYTECODE_TYPE_INTERFACE
        && !mmay_bytecode_type_is_extended_scalar((MarimayBytecodeType)value))
        || !mmay_bytecode_type_is_register((MarimayBytecodeType)value)) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    *type = (MarimayBytecodeType)value;
    return mmay_bytecode_nominal_decode_u32(cursor, layout);
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_record_layouts(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_count(
        cursor,
        function->options.max_record_layouts,
        &count
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        MarimaySymbolId symbol;
        MarimayBytecodeRecordField *fields = NULL;
        MarimayBytecodeRecordId id = MMAY_BYTECODE_NO_RECORD;
        size_t field_count = 0;
        size_t bytes = 0;
        size_t field_index = 0;
        status = mmay_bytecode_nominal_decode_symbol(cursor, &symbol);
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_record_fields
                    - function->record_fields.length,
                &field_count
            );
        }
        if (status != MMAY_BYTECODE_CODEC_OK
            || !mmay_size_multiply(field_count, sizeof(*fields), &bytes)) {
            return status != MMAY_BYTECODE_CODEC_OK
                ? status
                : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            fields = mmay_allocate(&function->allocator, bytes);
            if (fields == NULL) {
                return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
            memset(fields, 0, bytes);
        }
        for (field_index = 0; field_index < field_count; field_index += 1) {
            uint64_t mutable_wire = 0;
            status = mmay_bytecode_nominal_decode_type_layout(
                cursor,
                &fields[field_index].type,
                &fields[field_index].layout
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                status = mmay_bytecode_codec_decode_u64(cursor, &mutable_wire);
            }
            if (status != MMAY_BYTECODE_CODEC_OK || mutable_wire > 1) {
                status = status != MMAY_BYTECODE_CODEC_OK
                    ? status
                    : MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                break;
            }
            fields[field_index].is_mutable = mutable_wire != 0;
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && !mmay_bytecode_add_record_layout(
                function,
                symbol,
                fields,
                field_count,
                &id
            )) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        mmay_deallocate(&function->allocator, fields, bytes);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_enum_layouts(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_count(
        cursor,
        function->options.max_enum_layouts,
        &count
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        MarimaySymbolId symbol;
        MarimayBytecodeEnumVariant *variants = NULL;
        MarimayBytecodeEnumPayload *payloads = NULL;
        MarimayBytecodeEnumId id = MMAY_BYTECODE_NO_ENUM;
        size_t variant_count = 0;
        size_t payload_count = 0;
        size_t variant_bytes = 0;
        size_t payload_bytes = 0;
        size_t item = 0;
        status = mmay_bytecode_nominal_decode_symbol(cursor, &symbol);
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_enum_variants
                    - function->enum_variants.length,
                &variant_count
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_enum_payloads
                    - function->enum_payloads.length,
                &payload_count
            );
        }
        if (status != MMAY_BYTECODE_CODEC_OK
            || !mmay_size_multiply(
                variant_count,
                sizeof(*variants),
                &variant_bytes
            ) || !mmay_size_multiply(
                payload_count,
                sizeof(*payloads),
                &payload_bytes
            )) {
            return status != MMAY_BYTECODE_CODEC_OK
                ? status
                : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (variant_bytes != 0) {
            variants = mmay_allocate(&function->allocator, variant_bytes);
        }
        if (payload_bytes != 0) {
            payloads = mmay_allocate(&function->allocator, payload_bytes);
        }
        if ((variant_bytes != 0 && variants == NULL)
            || (payload_bytes != 0 && payloads == NULL)) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        for (item = 0; status == MMAY_BYTECODE_CODEC_OK && item < variant_count;
            item += 1) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                payload_count,
                &variants[item].payload_start
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                status = mmay_bytecode_nominal_decode_count(
                    cursor,
                    payload_count,
                    &variants[item].payload_count
                );
            }
            if (status == MMAY_BYTECODE_CODEC_OK
                && (variants[item].payload_start > payload_count
                    || variants[item].payload_count
                        > payload_count - variants[item].payload_start)) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
            }
        }
        for (item = 0; status == MMAY_BYTECODE_CODEC_OK && item < payload_count;
            item += 1) {
            status = mmay_bytecode_nominal_decode_type_layout(
                cursor,
                &payloads[item].type,
                &payloads[item].layout
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && !mmay_bytecode_add_enum_layout(
                function,
                symbol,
                variants,
                variant_count,
                payloads,
                payload_count,
                &id
            )) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        mmay_deallocate(&function->allocator, payloads, payload_bytes);
        mmay_deallocate(&function->allocator, variants, variant_bytes);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_interface_layouts(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_count(
        cursor,
        function->options.max_interface_layouts,
        &count
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        MarimaySymbolId symbol;
        MarimayBytecodeInterfaceId id = MMAY_BYTECODE_NO_INTERFACE;
        size_t method_count = 0;
        status = mmay_bytecode_nominal_decode_symbol(cursor, &symbol);
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_interface_methods,
                &method_count
            );
        }
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!mmay_bytecode_add_interface_layout(
                function,
                symbol,
                method_count,
                &id
            )) {
            return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if ((size_t)id != index) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_argument(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor,
    uint32_t *register_id,
    MarimayBytecodeType *type,
    uint32_t *layout
) {
    const MarimayBytecodeType *actual = NULL;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_u32(
        cursor,
        register_id
    );
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_decode_type_layout(cursor, type, layout);
    }
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    actual = *register_id < function->register_types.length
        ? mmay_vector_at_const(&function->register_types, *register_id)
        : NULL;
    if (actual == NULL || *actual != *type
        || mmay_bytecode_register_layout(function, *register_id) != *layout) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_record_constructions(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_count(
        cursor,
        function->options.max_record_constructions,
        &count
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        uint32_t record = MMAY_BYTECODE_NO_RECORD;
        MarimayBytecodeRecordArgument *arguments = NULL;
        MarimayBytecodeRecordConstructionId id =
            MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
        const MarimayBytecodeRecordLayout *record_layout = NULL;
        size_t argument_count = 0;
        size_t bytes = 0;
        size_t item = 0;
        status = mmay_bytecode_nominal_decode_u32(cursor, &record);
        record_layout = status == MMAY_BYTECODE_CODEC_OK
            ? mmay_bytecode_record_layout(function, record)
            : NULL;
        if (status == MMAY_BYTECODE_CODEC_OK && record_layout == NULL) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_record_arguments
                    - function->record_arguments.length,
                &argument_count
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && argument_count != record_layout->field_count) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status != MMAY_BYTECODE_CODEC_OK
            || !mmay_size_multiply(argument_count, sizeof(*arguments), &bytes)) {
            return status != MMAY_BYTECODE_CODEC_OK
                ? status
                : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            arguments = mmay_allocate(&function->allocator, bytes);
            if (arguments == NULL) {
                return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
        }
        for (item = 0; item < argument_count; item += 1) {
            status = mmay_bytecode_nominal_decode_argument(
                function,
                cursor,
                &arguments[item].register_id,
                &arguments[item].type,
                &arguments[item].layout
            );
            if (status != MMAY_BYTECODE_CODEC_OK) {
                break;
            }
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && (!mmay_vector_reserve(
                    &function->record_constructions,
                    function->record_constructions.length + 1
                ) || !mmay_vector_reserve(
                    &function->record_arguments,
                    function->record_arguments.length + argument_count
                ))) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && !mmay_bytecode_add_record_construction(
                function,
                record,
                arguments,
                argument_count,
                &id
            )) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        mmay_deallocate(&function->allocator, arguments, bytes);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_enum_constructions(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_count(
        cursor,
        function->options.max_enum_constructions,
        &count
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        uint32_t enum_id = MMAY_BYTECODE_NO_ENUM;
        MarimayBytecodeEnumArgument *arguments = NULL;
        MarimayBytecodeEnumConstructionId id = MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
        const MarimayBytecodeEnumLayout *layout = NULL;
        size_t variant = 0;
        size_t argument_count = 0;
        size_t bytes = 0;
        size_t item = 0;
        status = mmay_bytecode_nominal_decode_u32(cursor, &enum_id);
        layout = status == MMAY_BYTECODE_CODEC_OK
            ? mmay_bytecode_enum_layout(function, enum_id)
            : NULL;
        if (status == MMAY_BYTECODE_CODEC_OK && layout == NULL) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                layout->variant_count,
                &variant
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK && variant >= layout->variant_count) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_enum_arguments
                    - function->enum_arguments.length,
                &argument_count
            );
        }
        if (status != MMAY_BYTECODE_CODEC_OK
            || !mmay_size_multiply(argument_count, sizeof(*arguments), &bytes)) {
            return status != MMAY_BYTECODE_CODEC_OK
                ? status
                : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            arguments = mmay_allocate(&function->allocator, bytes);
            if (arguments == NULL) {
                return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
        }
        for (item = 0; item < argument_count; item += 1) {
            status = mmay_bytecode_nominal_decode_argument(
                function,
                cursor,
                &arguments[item].register_id,
                &arguments[item].type,
                &arguments[item].layout
            );
            if (status != MMAY_BYTECODE_CODEC_OK) {
                break;
            }
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && (!mmay_vector_reserve(
                    &function->enum_constructions,
                    function->enum_constructions.length + 1
                ) || !mmay_vector_reserve(
                    &function->enum_arguments,
                    function->enum_arguments.length + argument_count
                ))) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && !mmay_bytecode_add_enum_construction(
                function,
                enum_id,
                variant,
                arguments,
                argument_count,
                &id
            )) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        mmay_deallocate(&function->allocator, arguments, bytes);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_decode_interface_constructions(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    size_t count = 0;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_decode_count(
        cursor,
        function->options.max_interface_constructions,
        &count
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < count; index += 1) {
        uint32_t interface_id = MMAY_BYTECODE_NO_INTERFACE;
        uint32_t payload_register = UINT32_MAX;
        MarimayBytecodeType payload_type = MMAY_BYTECODE_TYPE_INVALID;
        uint32_t payload_layout = MMAY_BYTECODE_NO_LAYOUT;
        MarimaySymbolId *methods = NULL;
        MarimayBytecodeInterfaceConstructionId id =
            MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
        const MarimayBytecodeInterfaceLayout *layout = NULL;
        size_t method_count = 0;
        size_t bytes = 0;
        size_t item = 0;
        status = mmay_bytecode_nominal_decode_u32(cursor, &interface_id);
        layout = status == MMAY_BYTECODE_CODEC_OK
            ? mmay_bytecode_interface_layout(function, interface_id)
            : NULL;
        if (status == MMAY_BYTECODE_CODEC_OK && layout == NULL) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_u32(
                cursor,
                &payload_register
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_type_layout(
                cursor,
                &payload_type,
                &payload_layout
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_decode_count(
                cursor,
                function->options.max_interface_methods
                    - function->interface_methods.length,
                &method_count
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && method_count != layout->method_count) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (status != MMAY_BYTECODE_CODEC_OK
            || !mmay_size_multiply(method_count, sizeof(*methods), &bytes)) {
            return status != MMAY_BYTECODE_CODEC_OK
                ? status
                : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (bytes != 0) {
            methods = mmay_allocate(&function->allocator, bytes);
            if (methods == NULL) {
                return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
        }
        for (item = 0; item < method_count; item += 1) {
            status = mmay_bytecode_nominal_decode_symbol(cursor, &methods[item]);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                break;
            }
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && (!mmay_vector_reserve(
                    &function->interface_constructions,
                    function->interface_constructions.length + 1
                ) || !mmay_vector_reserve(
                    &function->interface_methods,
                    function->interface_methods.length + method_count
                ))) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        }
        if (status == MMAY_BYTECODE_CODEC_OK
            && !mmay_bytecode_add_interface_construction(
                function,
                interface_id,
                payload_register,
                payload_type,
                payload_layout,
                methods,
                method_count,
                &id
            )) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        mmay_deallocate(&function->allocator, methods, bytes);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if ((size_t)id != index) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}

MarimayBytecodeCodecStatus mmay_bytecode_decode_nominal_metadata(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    MarimayBytecodeCodecStatus status = MMAY_BYTECODE_CODEC_OK;
    if (function == NULL || cursor == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    status = mmay_bytecode_nominal_decode_record_layouts(function, cursor);
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_decode_enum_layouts(function, cursor);
    }
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_decode_interface_layouts(function, cursor);
    }
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_values_decode(function, cursor);
    }
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_decode_record_constructions(
            function,
            cursor
        );
    }
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_decode_enum_constructions(function, cursor);
    }
    if (status == MMAY_BYTECODE_CODEC_OK) {
        status = mmay_bytecode_nominal_decode_interface_constructions(
            function,
            cursor
        );
    }
    return status;
}
