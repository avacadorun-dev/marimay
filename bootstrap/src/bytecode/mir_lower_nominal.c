#include "mir_lower_internal.h"

#include <string.h>

bool ruda_mir_bytecode_type(
    const RudaMirFunction *function,
    RudaTypeId input,
    bool allow_void,
    bool allow_never,
    RudaBytecodeType *output
) {
    size_t index = 0;
    if (output == NULL) {
        return false;
    }
    if (input == RUDA_TYPE_BOOL) {
        *output = RUDA_BYTECODE_TYPE_BOOL;
        return true;
    }
    if (input == RUDA_TYPE_I64) {
        *output = RUDA_BYTECODE_TYPE_I64;
        return true;
    }
    if (input == RUDA_TYPE_F64) {
        *output = RUDA_BYTECODE_TYPE_F64;
        return true;
    }
    if (input == RUDA_TYPE_STRING) {
        *output = RUDA_BYTECODE_TYPE_STRING;
        return true;
    }
    if (allow_void && input == RUDA_TYPE_VOID) {
        *output = RUDA_BYTECODE_TYPE_VOID;
        return true;
    }
    if (allow_never && input == RUDA_TYPE_NEVER) {
        *output = RUDA_BYTECODE_TYPE_NEVER;
        return true;
    }
    if (function == NULL || input < RUDA_TYPE_FIRST_DYNAMIC) {
        return false;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const RudaMirArray *array = ruda_vector_at_const(
            &function->arrays,
            index
        );
        if (array == NULL || array->type != input) {
            continue;
        }
        if (array->element_type == RUDA_TYPE_F64) {
            *output = RUDA_BYTECODE_TYPE_ARRAY_F64;
            return true;
        }
        {
            size_t nested_index = 0;
            for (nested_index = 0; nested_index < function->arrays.length;
                nested_index += 1) {
                const RudaMirArray *nested = ruda_vector_at_const(
                    &function->arrays,
                    nested_index
                );
                if (nested != NULL && nested->type == array->element_type
                    && nested->element_type == RUDA_TYPE_F64) {
                    *output = RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64;
                    return true;
                }
            }
        }
        return false;
    }
    for (index = 0; index < function->records.length; index += 1) {
        const RudaMirRecord *record = ruda_vector_at_const(
            &function->records,
            index
        );
        if (record != NULL && record->type == input) {
            *output = RUDA_BYTECODE_TYPE_RECORD;
            return true;
        }
    }
    for (index = 0; index < function->enums.length; index += 1) {
        const RudaMirEnum *enumeration = ruda_vector_at_const(
            &function->enums,
            index
        );
        if (enumeration != NULL && enumeration->type == input) {
            *output = RUDA_BYTECODE_TYPE_ENUM;
            return true;
        }
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const RudaMirInterface *interface_layout = ruda_vector_at_const(
            &function->interfaces,
            index
        );
        if (interface_layout != NULL && interface_layout->type == input) {
            *output = RUDA_BYTECODE_TYPE_INTERFACE;
            return true;
        }
    }
    return false;
}

uint32_t ruda_mir_bytecode_layout_for_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    if (function == NULL || type < RUDA_TYPE_FIRST_DYNAMIC) {
        return RUDA_BYTECODE_NO_LAYOUT;
    }
    for (index = 0; index < function->records.length; index += 1) {
        const RudaMirRecord *record = ruda_vector_at_const(
            &function->records,
            index
        );
        if (record != NULL && record->type == type && index <= UINT32_MAX) {
            return (uint32_t)index;
        }
    }
    for (index = 0; index < function->enums.length; index += 1) {
        const RudaMirEnum *enumeration = ruda_vector_at_const(
            &function->enums,
            index
        );
        if (enumeration != NULL && enumeration->type == type
            && index <= UINT32_MAX) {
            return (uint32_t)index;
        }
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const RudaMirInterface *interface_layout = ruda_vector_at_const(
            &function->interfaces,
            index
        );
        if (interface_layout != NULL && interface_layout->type == type
            && index <= UINT32_MAX) {
            return (uint32_t)index;
        }
    }
    return RUDA_BYTECODE_NO_LAYOUT;
}

static RudaSymbolId ruda_mir_bytecode_nominal_symbol(
    RudaTypeId type,
    unsigned char tag
) {
    RudaSymbolId symbol;
    memset(&symbol, 0, sizeof(symbol));
    memcpy(symbol.bytes, &type, sizeof(type));
    symbol.bytes[sizeof(type)] = tag;
    return symbol;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_record_layouts(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output,
    RudaAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->records.length; index += 1) {
        const RudaMirRecord *record = ruda_vector_at_const(
            &input->records, index
        );
        RudaBytecodeRecordField *fields = NULL;
        size_t field_bytes = 0;
        size_t field_index = 0;
        RudaBytecodeRecordId record_id = RUDA_BYTECODE_NO_RECORD;
        if (record == NULL || !ruda_size_multiply(
                record->field_count, sizeof(*fields), &field_bytes
            )) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (field_bytes != 0) {
            fields = ruda_allocate(&allocator, field_bytes);
            if (fields == NULL) {
                return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(fields, 0, field_bytes);
        }
        for (field_index = 0; field_index < record->field_count;
            field_index += 1) {
            const RudaMirRecordField *field = ruda_vector_at_const(
                &input->record_fields, record->field_start + field_index
            );
            if (field == NULL || !ruda_mir_bytecode_type(
                    input, field->type, false, false, &fields[field_index].type
                )) {
                ruda_deallocate(&allocator, fields, field_bytes);
                return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            fields[field_index].layout = ruda_mir_bytecode_layout_for_type(
                input, field->type
            );
            fields[field_index].is_mutable = field->is_mutable;
        }
        if (!ruda_bytecode_add_record_layout(
                output,
                ruda_mir_bytecode_nominal_symbol(record->type, 0x52),
                fields,
                record->field_count,
                &record_id
            ) || record_id != index) {
            ruda_deallocate(&allocator, fields, field_bytes);
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_deallocate(&allocator, fields, field_bytes);
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_enum_layouts(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output,
    RudaAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->enums.length; index += 1) {
        const RudaMirEnum *enumeration = ruda_vector_at_const(
            &input->enums, index
        );
        RudaBytecodeEnumVariant *variants = NULL;
        RudaBytecodeEnumPayload *payloads = NULL;
        size_t variant_bytes = 0;
        size_t payload_bytes = 0;
        size_t variant_index = 0;
        size_t payload_index = 0;
        RudaBytecodeEnumId enum_id = RUDA_BYTECODE_NO_ENUM;
        if (enumeration == NULL || !ruda_size_multiply(
                enumeration->variant_count, sizeof(*variants), &variant_bytes
            ) || !ruda_size_multiply(
                enumeration->payload_count, sizeof(*payloads), &payload_bytes
            )) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (variant_bytes != 0) {
            variants = ruda_allocate(&allocator, variant_bytes);
            if (variants == NULL) {
                return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(variants, 0, variant_bytes);
        }
        if (payload_bytes != 0) {
            payloads = ruda_allocate(&allocator, payload_bytes);
            if (payloads == NULL) {
                ruda_deallocate(&allocator, variants, variant_bytes);
                return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(payloads, 0, payload_bytes);
        }
        for (variant_index = 0; variant_index < enumeration->variant_count;
            variant_index += 1) {
            const RudaMirEnumVariant *variant = ruda_vector_at_const(
                &input->enum_variants,
                enumeration->variant_start + variant_index
            );
            if (variant == NULL
                || variant->payload_start < enumeration->payload_start) {
                ruda_deallocate(&allocator, variants, variant_bytes);
                ruda_deallocate(&allocator, payloads, payload_bytes);
                return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
            variants[variant_index].payload_start =
                variant->payload_start - enumeration->payload_start;
            variants[variant_index].payload_count = variant->payload_count;
        }
        for (payload_index = 0; payload_index < enumeration->payload_count;
            payload_index += 1) {
            const RudaMirEnumPayload *payload = ruda_vector_at_const(
                &input->enum_payloads,
                enumeration->payload_start + payload_index
            );
            if (payload == NULL || !ruda_mir_bytecode_type(
                    input, payload->type, false, false,
                    &payloads[payload_index].type
                )) {
                ruda_deallocate(&allocator, variants, variant_bytes);
                ruda_deallocate(&allocator, payloads, payload_bytes);
                return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            payloads[payload_index].layout =
                ruda_mir_bytecode_layout_for_type(input, payload->type);
        }
        if (!ruda_bytecode_add_enum_layout(
                output,
                ruda_mir_bytecode_nominal_symbol(enumeration->type, 0x45),
                variants,
                enumeration->variant_count,
                payloads,
                enumeration->payload_count,
                &enum_id
            ) || enum_id != index) {
            ruda_deallocate(&allocator, variants, variant_bytes);
            ruda_deallocate(&allocator, payloads, payload_bytes);
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_deallocate(&allocator, variants, variant_bytes);
        ruda_deallocate(&allocator, payloads, payload_bytes);
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_interface_layouts(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output
) {
    size_t index = 0;
    for (index = 0; index < input->interfaces.length; index += 1) {
        const RudaMirInterface *layout = ruda_vector_at_const(
            &input->interfaces, index
        );
        RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
        if (layout == NULL) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (!ruda_bytecode_add_interface_layout(
                output,
                ruda_mir_bytecode_nominal_symbol(layout->type, 0x49),
                layout->method_count,
                &interface_id
            ) || interface_id != index) {
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_record_constructions(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output,
    RudaAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->record_constructions.length; index += 1) {
        const RudaMirRecordConstruction *construction = ruda_vector_at_const(
            &input->record_constructions, index
        );
        RudaBytecodeRecordArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        RudaBytecodeRecordConstructionId construction_id =
            RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
        if (construction == NULL || !ruda_size_multiply(
                construction->argument_count, sizeof(*arguments),
                &argument_bytes
            )) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (argument_bytes != 0) {
            arguments = ruda_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(arguments, 0, argument_bytes);
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaMirRecordArgument *argument = ruda_vector_at_const(
                &input->record_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !ruda_mir_bytecode_type(
                    input, argument->type, false, false,
                    &arguments[argument_index].type
                )) {
                ruda_deallocate(&allocator, arguments, argument_bytes);
                return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout =
                ruda_mir_bytecode_layout_for_type(input, argument->type);
        }
        if (!ruda_bytecode_add_record_construction(
                output, construction->record, arguments,
                construction->argument_count, &construction_id
            ) || construction_id != index) {
            ruda_deallocate(&allocator, arguments, argument_bytes);
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_deallocate(&allocator, arguments, argument_bytes);
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_enum_constructions(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output,
    RudaAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->enum_constructions.length; index += 1) {
        const RudaMirEnumConstruction *construction = ruda_vector_at_const(
            &input->enum_constructions, index
        );
        RudaBytecodeEnumArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        RudaBytecodeEnumConstructionId construction_id =
            RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
        if (construction == NULL || !ruda_size_multiply(
                construction->argument_count, sizeof(*arguments),
                &argument_bytes
            )) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (argument_bytes != 0) {
            arguments = ruda_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(arguments, 0, argument_bytes);
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const RudaMirEnumArgument *argument = ruda_vector_at_const(
                &input->enum_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !ruda_mir_bytecode_type(
                    input, argument->type, false, false,
                    &arguments[argument_index].type
                )) {
                ruda_deallocate(&allocator, arguments, argument_bytes);
                return RUDA_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout =
                ruda_mir_bytecode_layout_for_type(input, argument->type);
        }
        if (!ruda_bytecode_add_enum_construction(
                output, construction->enum_id, construction->variant,
                arguments, construction->argument_count, &construction_id
            ) || construction_id != index) {
            ruda_deallocate(&allocator, arguments, argument_bytes);
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        ruda_deallocate(&allocator, arguments, argument_bytes);
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

static RudaMirBytecodeLowerStatus ruda_mir_bytecode_interface_constructions(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output
) {
    size_t index = 0;
    for (index = 0; index < input->interface_constructions.length;
        index += 1) {
        const RudaMirInterfaceConstruction *construction =
            ruda_vector_at_const(&input->interface_constructions, index);
        const RudaSymbolId *methods = NULL;
        size_t method_end = 0;
        RudaBytecodeType payload_type;
        RudaBytecodeInterfaceConstructionId construction_id =
            RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
        if (construction == NULL || !ruda_size_add(
                construction->method_start,
                construction->method_count,
                &method_end
            ) || method_end > input->interface_methods.length
            || !ruda_mir_bytecode_type(
                input, construction->payload_type, false, false, &payload_type
            )) {
            return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (construction->method_count != 0) {
            methods = ruda_vector_at_const(
                &input->interface_methods, construction->method_start
            );
            if (methods == NULL) {
                return RUDA_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
        }
        if (!ruda_bytecode_add_interface_construction(
                output,
                construction->interface_id,
                construction->payload,
                payload_type,
                ruda_mir_bytecode_layout_for_type(
                    input, construction->payload_type
                ),
                methods,
                construction->method_count,
                &construction_id
            ) || construction_id != index) {
            return RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
    }
    return RUDA_MIR_BYTECODE_LOWER_OK;
}

RudaMirBytecodeLowerStatus ruda_mir_bytecode_lower_nominals(
    const RudaMirFunction *input,
    RudaBytecodeFunction *output,
    RudaAllocator allocator
) {
    RudaMirBytecodeLowerStatus status;
    if (input == NULL || output == NULL) {
        return RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    status = ruda_mir_bytecode_record_layouts(input, output, allocator);
    if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
        status = ruda_mir_bytecode_enum_layouts(input, output, allocator);
    }
    if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
        status = ruda_mir_bytecode_interface_layouts(input, output);
    }
    if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
        status = ruda_mir_bytecode_record_constructions(
            input, output, allocator
        );
    }
    if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
        status = ruda_mir_bytecode_enum_constructions(
            input, output, allocator
        );
    }
    if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
        status = ruda_mir_bytecode_interface_constructions(input, output);
    }
    return status;
}
