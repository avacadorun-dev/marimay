#include "mir_lower_internal.h"

#include <string.h>

static bool mmay_mir_bytecode_array_is_generic(
    const MarimayMirFunction *function,
    const MarimayMirArray *array
) {
    size_t index = 0;
    if (function == NULL || array == NULL || array->element_type == MMAY_TYPE_F64) {
        return false;
    }
    if (array->element_type < MMAY_TYPE_FIRST_DYNAMIC) {
        return true;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *nested = mmay_vector_at_const(
            &function->arrays, index
        );
        if (nested != NULL && nested->type == array->element_type) {
            return nested->element_type != MMAY_TYPE_F64;
        }
    }
    return true;
}

bool mmay_mir_bytecode_type(
    const MarimayMirFunction *function,
    MarimayTypeId input,
    bool allow_void,
    bool allow_never,
    MarimayBytecodeType *output
) {
    size_t index = 0;
    if (output == NULL) {
        return false;
    }
    if (input == MMAY_TYPE_BOOL) {
        *output = MMAY_BYTECODE_TYPE_BOOL;
        return true;
    }
    if (input == MMAY_TYPE_I64) {
        *output = MMAY_BYTECODE_TYPE_I64;
        return true;
    }
    if (input == MMAY_TYPE_I8) {
        *output = MMAY_BYTECODE_TYPE_I8;
        return true;
    }
    if (input == MMAY_TYPE_I16) {
        *output = MMAY_BYTECODE_TYPE_I16;
        return true;
    }
    if (input == MMAY_TYPE_I32) {
        *output = MMAY_BYTECODE_TYPE_I32;
        return true;
    }
    if (input == MMAY_TYPE_U8) {
        *output = MMAY_BYTECODE_TYPE_U8;
        return true;
    }
    if (input == MMAY_TYPE_U16) {
        *output = MMAY_BYTECODE_TYPE_U16;
        return true;
    }
    if (input == MMAY_TYPE_U32) {
        *output = MMAY_BYTECODE_TYPE_U32;
        return true;
    }
    if (input == MMAY_TYPE_RUNE) {
        *output = MMAY_BYTECODE_TYPE_U32;
        return true;
    }
    if (input == MMAY_TYPE_U64) {
        *output = MMAY_BYTECODE_TYPE_U64;
        return true;
    }
    if (input == MMAY_TYPE_F64) {
        *output = MMAY_BYTECODE_TYPE_F64;
        return true;
    }
    if (input == MMAY_TYPE_F32) {
        *output = MMAY_BYTECODE_TYPE_F32;
        return true;
    }
    if (input == MMAY_TYPE_STRING) {
        *output = MMAY_BYTECODE_TYPE_STRING;
        return true;
    }
    if (allow_void && input == MMAY_TYPE_VOID) {
        *output = MMAY_BYTECODE_TYPE_VOID;
        return true;
    }
    if (allow_never && input == MMAY_TYPE_NEVER) {
        *output = MMAY_BYTECODE_TYPE_NEVER;
        return true;
    }
    if (function == NULL || input < MMAY_TYPE_FIRST_DYNAMIC) {
        return false;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &function->arrays,
            index
        );
        if (array == NULL || array->type != input) {
            continue;
        }
        if (!mmay_mir_bytecode_array_is_generic(function, array)
            && array->element_type == MMAY_TYPE_F64) {
            *output = MMAY_BYTECODE_TYPE_ARRAY_F64;
            return true;
        }
        if (!mmay_mir_bytecode_array_is_generic(function, array)
            && array->element_type >= MMAY_TYPE_FIRST_DYNAMIC) {
            size_t nested_index = 0;
            for (nested_index = 0; nested_index < function->arrays.length;
                nested_index += 1) {
                const MarimayMirArray *nested = mmay_vector_at_const(
                    &function->arrays, nested_index
                );
                if (nested != NULL && nested->type == array->element_type
                    && nested->element_type == MMAY_TYPE_F64) {
                    *output = MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64;
                    return true;
                }
            }
        }
        *output = MMAY_BYTECODE_TYPE_ARRAY;
        return true;
    }
    for (index = 0; index < function->records.length; index += 1) {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &function->records,
            index
        );
        if (record != NULL && record->type == input) {
            *output = MMAY_BYTECODE_TYPE_RECORD;
            return true;
        }
    }
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *enumeration = mmay_vector_at_const(
            &function->enums,
            index
        );
        if (enumeration != NULL && enumeration->type == input) {
            *output = MMAY_BYTECODE_TYPE_ENUM;
            return true;
        }
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const MarimayMirInterface *interface_layout = mmay_vector_at_const(
            &function->interfaces,
            index
        );
        if (interface_layout != NULL && interface_layout->type == input) {
            *output = MMAY_BYTECODE_TYPE_INTERFACE;
            return true;
        }
    }
    return false;
}

uint32_t mmay_mir_bytecode_layout_for_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    if (function == NULL || type < MMAY_TYPE_FIRST_DYNAMIC) {
        return MMAY_BYTECODE_NO_LAYOUT;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &function->arrays, index
        );
        if (array != NULL && array->type == type && index <= UINT32_MAX
            && mmay_mir_bytecode_array_is_generic(function, array)) {
            return (uint32_t)index;
        }
    }
    for (index = 0; index < function->records.length; index += 1) {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &function->records,
            index
        );
        if (record != NULL && record->type == type && index <= UINT32_MAX) {
            return (uint32_t)index;
        }
    }
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *enumeration = mmay_vector_at_const(
            &function->enums,
            index
        );
        if (enumeration != NULL && enumeration->type == type
            && index <= UINT32_MAX) {
            return (uint32_t)index;
        }
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const MarimayMirInterface *interface_layout = mmay_vector_at_const(
            &function->interfaces,
            index
        );
        if (interface_layout != NULL && interface_layout->type == type
            && index <= UINT32_MAX) {
            return (uint32_t)index;
        }
    }
    return MMAY_BYTECODE_NO_LAYOUT;
}

static MarimaySymbolId mmay_mir_bytecode_nominal_symbol(
    MarimayTypeId type,
    unsigned char tag
) {
    MarimaySymbolId symbol;
    memset(&symbol, 0, sizeof(symbol));
    memcpy(symbol.bytes, &type, sizeof(type));
    symbol.bytes[sizeof(type)] = tag;
    return symbol;
}

static MarimaySymbolId mmay_mir_bytecode_enum_symbol(
    const MarimayMirFunction *function,
    const MarimayMirEnum *enumeration
) {
    MarimaySymbolId option_base;
    MarimaySymbolId specialized;
    const MarimayMirEnumVariant *some = enumeration != NULL
            && enumeration->variant_count == 2
        ? mmay_vector_at_const(
            &function->enum_variants, enumeration->variant_start
        ) : NULL;
    const MarimayMirEnumVariant *none = enumeration != NULL
            && enumeration->variant_count == 2
        ? mmay_vector_at_const(
            &function->enum_variants, enumeration->variant_start + 1
        ) : NULL;
    const MarimayMirEnumPayload *payload = enumeration != NULL && some != NULL
            && some->payload_count == 1
        ? mmay_vector_at_const(
            &function->enum_payloads,
            some->payload_start
        ) : NULL;
    MarimayBytecodeType payload_type = MMAY_BYTECODE_TYPE_INVALID;
    memset(&option_base, 0, sizeof(option_base));
    memset(&specialized, 0, sizeof(specialized));
    if (enumeration != NULL
        && mmay_symbol_id_from_bytes(
            "std", 3, "prelude", 7,
            (const unsigned char *)"Option", 6, 0, &option_base
        ) && mmay_symbol_id_equal(enumeration->symbol, option_base)
        && enumeration->payload_count == 1 && some != NULL
        && some->kind == MMAY_MIR_ENUM_VARIANT_POSITIONAL
        && some->payload_start == enumeration->payload_start
        && some->payload_count == 1
        && none != NULL && none->kind == MMAY_MIR_ENUM_VARIANT_UNIT
        && none->payload_count == 0 && payload != NULL
        && mmay_mir_bytecode_type(
            function, payload->type, false, false, &payload_type
        ) && mmay_bytecode_numeric_option_symbol(
            payload_type, &specialized
        )) {
        return specialized;
    }
    return enumeration != NULL
        ? mmay_mir_bytecode_nominal_symbol(enumeration->type, 0x45)
        : specialized;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_record_layouts(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output,
    MarimayAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->records.length; index += 1) {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &input->records, index
        );
        MarimayBytecodeRecordField *fields = NULL;
        size_t field_bytes = 0;
        size_t field_index = 0;
        MarimayBytecodeRecordId record_id = MMAY_BYTECODE_NO_RECORD;
        if (record == NULL || !mmay_size_multiply(
                record->field_count, sizeof(*fields), &field_bytes
            )) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (field_bytes != 0) {
            fields = mmay_allocate(&allocator, field_bytes);
            if (fields == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(fields, 0, field_bytes);
        }
        for (field_index = 0; field_index < record->field_count;
            field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &input->record_fields, record->field_start + field_index
            );
            if (field == NULL || !mmay_mir_bytecode_type(
                    input, field->type, false, false, &fields[field_index].type
                )) {
                mmay_deallocate(&allocator, fields, field_bytes);
                return MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            fields[field_index].layout = mmay_mir_bytecode_layout_for_type(
                input, field->type
            );
            fields[field_index].is_mutable = field->is_mutable;
        }
        if (!mmay_bytecode_add_record_layout(
                output,
                mmay_mir_bytecode_nominal_symbol(record->type, 0x52),
                fields,
                record->field_count,
                &record_id
            ) || record_id != index) {
            mmay_deallocate(&allocator, fields, field_bytes);
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_deallocate(&allocator, fields, field_bytes);
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_enum_layouts(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output,
    MarimayAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->enums.length; index += 1) {
        const MarimayMirEnum *enumeration = mmay_vector_at_const(
            &input->enums, index
        );
        MarimayBytecodeEnumVariant *variants = NULL;
        MarimayBytecodeEnumPayload *payloads = NULL;
        size_t variant_bytes = 0;
        size_t payload_bytes = 0;
        size_t variant_index = 0;
        size_t payload_index = 0;
        MarimayBytecodeEnumId enum_id = MMAY_BYTECODE_NO_ENUM;
        if (enumeration == NULL || !mmay_size_multiply(
                enumeration->variant_count, sizeof(*variants), &variant_bytes
            ) || !mmay_size_multiply(
                enumeration->payload_count, sizeof(*payloads), &payload_bytes
            )) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (variant_bytes != 0) {
            variants = mmay_allocate(&allocator, variant_bytes);
            if (variants == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(variants, 0, variant_bytes);
        }
        if (payload_bytes != 0) {
            payloads = mmay_allocate(&allocator, payload_bytes);
            if (payloads == NULL) {
                mmay_deallocate(&allocator, variants, variant_bytes);
                return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(payloads, 0, payload_bytes);
        }
        for (variant_index = 0; variant_index < enumeration->variant_count;
            variant_index += 1) {
            const MarimayMirEnumVariant *variant = mmay_vector_at_const(
                &input->enum_variants,
                enumeration->variant_start + variant_index
            );
            if (variant == NULL
                || variant->payload_start < enumeration->payload_start) {
                mmay_deallocate(&allocator, variants, variant_bytes);
                mmay_deallocate(&allocator, payloads, payload_bytes);
                return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
            variants[variant_index].payload_start =
                variant->payload_start - enumeration->payload_start;
            variants[variant_index].payload_count = variant->payload_count;
        }
        for (payload_index = 0; payload_index < enumeration->payload_count;
            payload_index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &input->enum_payloads,
                enumeration->payload_start + payload_index
            );
            if (payload == NULL || !mmay_mir_bytecode_type(
                    input, payload->type, false, false,
                    &payloads[payload_index].type
                )) {
                mmay_deallocate(&allocator, variants, variant_bytes);
                mmay_deallocate(&allocator, payloads, payload_bytes);
                return MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            payloads[payload_index].layout =
                mmay_mir_bytecode_layout_for_type(input, payload->type);
        }
        if (!mmay_bytecode_add_enum_layout(
                output,
                mmay_mir_bytecode_enum_symbol(input, enumeration),
                variants,
                enumeration->variant_count,
                payloads,
                enumeration->payload_count,
                &enum_id
            ) || enum_id != index) {
            mmay_deallocate(&allocator, variants, variant_bytes);
            mmay_deallocate(&allocator, payloads, payload_bytes);
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_deallocate(&allocator, variants, variant_bytes);
        mmay_deallocate(&allocator, payloads, payload_bytes);
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_interface_layouts(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output
) {
    size_t index = 0;
    for (index = 0; index < input->interfaces.length; index += 1) {
        const MarimayMirInterface *layout = mmay_vector_at_const(
            &input->interfaces, index
        );
        MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
        if (layout == NULL) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (!mmay_bytecode_add_interface_layout(
                output,
                mmay_mir_bytecode_nominal_symbol(layout->type, 0x49),
                layout->method_count,
                &interface_id
            ) || interface_id != index) {
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_record_constructions(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output,
    MarimayAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->record_constructions.length; index += 1) {
        const MarimayMirRecordConstruction *construction = mmay_vector_at_const(
            &input->record_constructions, index
        );
        MarimayBytecodeRecordArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        MarimayBytecodeRecordConstructionId construction_id =
            MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
        if (construction == NULL || !mmay_size_multiply(
                construction->argument_count, sizeof(*arguments),
                &argument_bytes
            )) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (argument_bytes != 0) {
            arguments = mmay_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(arguments, 0, argument_bytes);
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayMirRecordArgument *argument = mmay_vector_at_const(
                &input->record_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !mmay_mir_bytecode_type(
                    input, argument->type, false, false,
                    &arguments[argument_index].type
                )) {
                mmay_deallocate(&allocator, arguments, argument_bytes);
                return MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout =
                mmay_mir_bytecode_layout_for_type(input, argument->type);
        }
        if (!mmay_bytecode_add_record_construction(
                output, construction->record, arguments,
                construction->argument_count, &construction_id
            ) || construction_id != index) {
            mmay_deallocate(&allocator, arguments, argument_bytes);
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_deallocate(&allocator, arguments, argument_bytes);
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_enum_constructions(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output,
    MarimayAllocator allocator
) {
    size_t index = 0;
    for (index = 0; index < input->enum_constructions.length; index += 1) {
        const MarimayMirEnumConstruction *construction = mmay_vector_at_const(
            &input->enum_constructions, index
        );
        MarimayBytecodeEnumArgument *arguments = NULL;
        size_t argument_bytes = 0;
        size_t argument_index = 0;
        MarimayBytecodeEnumConstructionId construction_id =
            MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
        if (construction == NULL || !mmay_size_multiply(
                construction->argument_count, sizeof(*arguments),
                &argument_bytes
            )) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (argument_bytes != 0) {
            arguments = mmay_allocate(&allocator, argument_bytes);
            if (arguments == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
            }
            memset(arguments, 0, argument_bytes);
        }
        for (argument_index = 0;
            argument_index < construction->argument_count;
            argument_index += 1) {
            const MarimayMirEnumArgument *argument = mmay_vector_at_const(
                &input->enum_arguments,
                construction->argument_start + argument_index
            );
            if (argument == NULL || !mmay_mir_bytecode_type(
                    input, argument->type, false, false,
                    &arguments[argument_index].type
                )) {
                mmay_deallocate(&allocator, arguments, argument_bytes);
                return MMAY_MIR_BYTECODE_LOWER_UNSUPPORTED_TYPE;
            }
            arguments[argument_index].register_id = argument->register_id;
            arguments[argument_index].layout =
                mmay_mir_bytecode_layout_for_type(input, argument->type);
        }
        if (!mmay_bytecode_add_enum_construction(
                output, construction->enum_id, construction->variant,
                arguments, construction->argument_count, &construction_id
            ) || construction_id != index) {
            mmay_deallocate(&allocator, arguments, argument_bytes);
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
        mmay_deallocate(&allocator, arguments, argument_bytes);
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

static MarimayMirBytecodeLowerStatus mmay_mir_bytecode_interface_constructions(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output
) {
    size_t index = 0;
    for (index = 0; index < input->interface_constructions.length;
        index += 1) {
        const MarimayMirInterfaceConstruction *construction =
            mmay_vector_at_const(&input->interface_constructions, index);
        const MarimaySymbolId *methods = NULL;
        size_t method_end = 0;
        MarimayBytecodeType payload_type;
        MarimayBytecodeInterfaceConstructionId construction_id =
            MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
        if (construction == NULL || !mmay_size_add(
                construction->method_start,
                construction->method_count,
                &method_end
            ) || method_end > input->interface_methods.length
            || !mmay_mir_bytecode_type(
                input, construction->payload_type, false, false, &payload_type
            )) {
            return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
        }
        if (construction->method_count != 0) {
            methods = mmay_vector_at_const(
                &input->interface_methods, construction->method_start
            );
            if (methods == NULL) {
                return MMAY_MIR_BYTECODE_LOWER_INVALID_MIR;
            }
        }
        if (!mmay_bytecode_add_interface_construction(
                output,
                construction->interface_id,
                construction->payload,
                payload_type,
                mmay_mir_bytecode_layout_for_type(
                    input, construction->payload_type
                ),
                methods,
                construction->method_count,
                &construction_id
            ) || construction_id != index) {
            return MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED;
        }
    }
    return MMAY_MIR_BYTECODE_LOWER_OK;
}

MarimayMirBytecodeLowerStatus mmay_mir_bytecode_lower_nominals(
    const MarimayMirFunction *input,
    MarimayBytecodeFunction *output,
    MarimayAllocator allocator
) {
    MarimayMirBytecodeLowerStatus status;
    if (input == NULL || output == NULL) {
        return MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT;
    }
    status = mmay_mir_bytecode_record_layouts(input, output, allocator);
    if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
        status = mmay_mir_bytecode_enum_layouts(input, output, allocator);
    }
    if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
        status = mmay_mir_bytecode_interface_layouts(input, output);
    }
    if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
        status = mmay_mir_bytecode_record_constructions(
            input, output, allocator
        );
    }
    if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
        status = mmay_mir_bytecode_enum_constructions(
            input, output, allocator
        );
    }
    if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
        status = mmay_mir_bytecode_interface_constructions(input, output);
    }
    return status;
}
