#include "mir_internal.h"
#include "mir_collections_internal.h"
#include "mmay/base/utf8.h"
#include <string.h>
bool mmay_mir_reference_is_invalid(MarimayMirReference reference) {
    return reference.execution_provenance == UINT64_MAX
        && reference.slot == UINT32_MAX;
}
bool mmay_mir_reference_equal(
    MarimayMirReference left,
    MarimayMirReference right
) {
    return left.execution_provenance == right.execution_provenance
        && left.slot == right.slot;
}
MarimayMirFunctionOptions mmay_mir_function_default_options(void) {
    MarimayMirFunctionOptions options;
    options.max_registers = 100000;
    options.max_blocks = 10000;
    options.max_instructions = 1000000;
    options.max_calls = 100000;
    options.max_call_arguments = 1000000;
    options.max_record_layouts = 100000;
    options.max_record_fields = 1000000;
    options.max_record_constructions = 100000;
    options.max_record_arguments = 1000000;
    options.max_enum_layouts = 100000;
    options.max_enum_variants = 1000000;
    options.max_enum_payloads = 1000000;
    options.max_enum_constructions = 100000;
    options.max_enum_arguments = 1000000;
    options.max_string_constants = 100000;
    options.max_string_bytes = 16U * 1024U * 1024U;
    options.max_array_layouts = 100000;
    options.max_array_constructions = 100000;
    options.max_array_arguments = 1000000;
    options.max_collection_layouts = 100000;
    options.max_interface_layouts = 100000;
    options.max_interface_constructions = 100000;
    options.max_interface_methods = 1000000;
    options.max_verify_work = 100000000;
    return options;
}
bool mmay_mir_function_init(
    MarimayMirFunction *function,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    const MarimayTypeId *parameter_types,
    size_t parameter_count,
    MarimayTypeId result_type,
    MarimayMirFunctionOptions options
) {
    size_t index = 0;
    bool registers_initialized = false;
    bool blocks_initialized = false;
    bool calls_initialized = false;
    bool call_arguments_initialized = false;
    bool records_initialized = false;
    bool record_fields_initialized = false;
    bool record_constructions_initialized = false;
    bool record_arguments_initialized = false;
    bool enums_initialized = false;
    bool enum_variants_initialized = false;
    bool enum_payloads_initialized = false;
    bool enum_constructions_initialized = false;
    bool enum_arguments_initialized = false;
    bool string_constants_initialized = false;
    bool arrays_initialized = false;
    bool array_constructions_initialized = false;
    bool array_arguments_initialized = false;
    bool collections_initialized = false;
    bool interfaces_initialized = false;
    bool interface_constructions_initialized = false;
    bool interface_methods_initialized = false;
    if (function == NULL || allocator.allocate == NULL
        || allocator.deallocate == NULL
        || (parameter_types == NULL && parameter_count != 0)
        || parameter_count > options.max_registers
        || parameter_count > UINT32_MAX
        || result_type == MMAY_TYPE_INVALID || options.max_registers == 0
        || options.max_registers > UINT32_MAX || options.max_blocks == 0
        || options.max_blocks > UINT32_MAX || options.max_instructions == 0
        || options.max_calls == 0 || options.max_calls > UINT32_MAX
        || options.max_call_arguments == 0
        || options.max_record_layouts == 0
        || options.max_record_layouts > UINT32_MAX
        || options.max_record_fields == 0
        || options.max_record_constructions == 0
        || options.max_record_constructions > UINT32_MAX
        || options.max_record_arguments == 0
        || options.max_enum_layouts == 0
        || options.max_enum_layouts > UINT32_MAX
        || options.max_enum_variants == 0
        || options.max_enum_payloads == 0
        || options.max_enum_constructions == 0
        || options.max_enum_constructions > UINT32_MAX
        || options.max_enum_arguments == 0
        || options.max_string_constants == 0
        || options.max_string_constants > UINT32_MAX
        || options.max_string_bytes == 0
        || options.max_array_layouts == 0
        || options.max_array_layouts > UINT32_MAX
        || options.max_array_constructions == 0
        || options.max_array_constructions > UINT32_MAX
        || options.max_array_arguments == 0
        || options.max_collection_layouts == 0
        || options.max_collection_layouts > UINT32_MAX
        || options.max_interface_layouts == 0
        || options.max_interface_layouts > UINT32_MAX
        || options.max_interface_constructions == 0
        || options.max_interface_constructions > UINT32_MAX
        || options.max_interface_methods == 0
        || options.max_verify_work == 0) {
        return false;
    }
    for (index = 0; index < parameter_count; index += 1) {
        if (parameter_types[index] == MMAY_TYPE_INVALID) {
            return false;
        }
    }
    memset(function, 0, sizeof(*function));
    registers_initialized = mmay_vector_init(
        &function->register_types,
        allocator,
        sizeof(MarimayTypeId)
    );
    blocks_initialized = mmay_vector_init(
        &function->blocks,
        allocator,
        sizeof(MarimayMirBlock)
    );
    calls_initialized = mmay_vector_init(
        &function->calls,
        allocator,
        sizeof(MarimayMirCall)
    );
    call_arguments_initialized = mmay_vector_init(
        &function->call_arguments,
        allocator,
        sizeof(MarimayMirCallArgument)
    );
    records_initialized = mmay_vector_init(
        &function->records,
        allocator,
        sizeof(MarimayMirRecord)
    );
    record_fields_initialized = mmay_vector_init(
        &function->record_fields,
        allocator,
        sizeof(MarimayMirRecordField)
    );
    record_constructions_initialized = mmay_vector_init(
        &function->record_constructions,
        allocator,
        sizeof(MarimayMirRecordConstruction)
    );
    record_arguments_initialized = mmay_vector_init(
        &function->record_arguments,
        allocator,
        sizeof(MarimayMirRecordArgument)
    );
    enums_initialized = mmay_vector_init(
        &function->enums,
        allocator,
        sizeof(MarimayMirEnum)
    );
    enum_variants_initialized = mmay_vector_init(
        &function->enum_variants,
        allocator,
        sizeof(MarimayMirEnumVariant)
    );
    enum_payloads_initialized = mmay_vector_init(
        &function->enum_payloads,
        allocator,
        sizeof(MarimayMirEnumPayload)
    );
    enum_constructions_initialized = mmay_vector_init(
        &function->enum_constructions,
        allocator,
        sizeof(MarimayMirEnumConstruction)
    );
    enum_arguments_initialized = mmay_vector_init(
        &function->enum_arguments,
        allocator,
        sizeof(MarimayMirEnumArgument)
    );
    string_constants_initialized = mmay_vector_init(
        &function->string_constants,
        allocator,
        sizeof(MarimayMirStringConstant)
    );
    arrays_initialized = mmay_vector_init(
        &function->arrays,
        allocator,
        sizeof(MarimayMirArray)
    );
    array_constructions_initialized = mmay_vector_init(
        &function->array_constructions,
        allocator,
        sizeof(MarimayMirArrayConstruction)
    );
    array_arguments_initialized = mmay_vector_init(
        &function->array_arguments,
        allocator,
        sizeof(MarimayMirArrayArgument)
    );
    collections_initialized = mmay_mir_collections_init(function, allocator);
    interfaces_initialized = mmay_vector_init(
        &function->interfaces, allocator, sizeof(MarimayMirInterface)
    );
    interface_constructions_initialized = mmay_vector_init(
        &function->interface_constructions, allocator,
        sizeof(MarimayMirInterfaceConstruction)
    );
    interface_methods_initialized = mmay_vector_init(
        &function->interface_methods, allocator, sizeof(MarimaySymbolId)
    );
    if (!registers_initialized || !blocks_initialized || !calls_initialized
        || !call_arguments_initialized || !records_initialized
        || !record_fields_initialized || !record_constructions_initialized
        || !record_arguments_initialized || !enums_initialized
        || !enum_variants_initialized || !enum_payloads_initialized
        || !enum_constructions_initialized || !enum_arguments_initialized
        || !string_constants_initialized || !arrays_initialized
        || !array_constructions_initialized || !array_arguments_initialized
        || !collections_initialized
        || !interfaces_initialized || !interface_constructions_initialized
        || !interface_methods_initialized
        || (parameter_count != 0 && !mmay_vector_append(
            &function->register_types,
            parameter_types,
            parameter_count
        ))) {
        if (collections_initialized) mmay_mir_collections_dispose(function);
        if (interface_methods_initialized) {
            mmay_vector_dispose(&function->interface_methods);
        }
        if (interface_constructions_initialized) {
            mmay_vector_dispose(&function->interface_constructions);
        }
        if (interfaces_initialized) {
            mmay_vector_dispose(&function->interfaces);
        }
        if (array_arguments_initialized) {
            mmay_vector_dispose(&function->array_arguments);
        }
        if (array_constructions_initialized) {
            mmay_vector_dispose(&function->array_constructions);
        }
        if (arrays_initialized) {
            mmay_vector_dispose(&function->arrays);
        }
        if (string_constants_initialized) {
            mmay_vector_dispose(&function->string_constants);
        }
        if (enum_arguments_initialized) {
            mmay_vector_dispose(&function->enum_arguments);
        }
        if (enum_constructions_initialized) {
            mmay_vector_dispose(&function->enum_constructions);
        }
        if (enum_payloads_initialized) {
            mmay_vector_dispose(&function->enum_payloads);
        }
        if (enum_variants_initialized) {
            mmay_vector_dispose(&function->enum_variants);
        }
        if (enums_initialized) {
            mmay_vector_dispose(&function->enums);
        }
        if (record_arguments_initialized) {
            mmay_vector_dispose(&function->record_arguments);
        }
        if (record_constructions_initialized) {
            mmay_vector_dispose(&function->record_constructions);
        }
        if (record_fields_initialized) {
            mmay_vector_dispose(&function->record_fields);
        }
        if (records_initialized) {
            mmay_vector_dispose(&function->records);
        }
        if (call_arguments_initialized) {
            mmay_vector_dispose(&function->call_arguments);
        }
        if (calls_initialized) {
            mmay_vector_dispose(&function->calls);
        }
        if (blocks_initialized) {
            mmay_vector_dispose(&function->blocks);
        }
        if (registers_initialized) {
            mmay_vector_dispose(&function->register_types);
        }
        memset(function, 0, sizeof(*function));
        return false;
    }
    function->allocator = allocator;
    function->symbol = symbol;
    function->result_type = result_type;
    function->parameter_count = parameter_count;
    function->options = options;
    function->initialized = true;
    return true;
}
void mmay_mir_function_dispose(MarimayMirFunction *function) {
    size_t index = 0;
    if (function == NULL || !function->initialized) {
        return;
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        MarimayMirBlock *block = mmay_vector_at(&function->blocks, index);
        if (block != NULL && block->initialized) {
            mmay_vector_dispose(&block->instructions);
            memset(block, 0, sizeof(*block));
        }
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        MarimayMirStringConstant *constant = mmay_vector_at(
            &function->string_constants,
            index
        );
        if (constant != NULL) {
            mmay_owned_bytes_dispose(&constant->value);
        }
    }
    mmay_vector_dispose(&function->string_constants);
    mmay_vector_dispose(&function->interface_methods);
    mmay_vector_dispose(&function->interface_constructions);
    mmay_vector_dispose(&function->interfaces);
    mmay_vector_dispose(&function->array_arguments);
    mmay_mir_collections_dispose(function);
    mmay_vector_dispose(&function->array_constructions);
    mmay_vector_dispose(&function->arrays);
    mmay_vector_dispose(&function->call_arguments);
    mmay_vector_dispose(&function->calls);
    mmay_vector_dispose(&function->record_arguments);
    mmay_vector_dispose(&function->record_constructions);
    mmay_vector_dispose(&function->record_fields);
    mmay_vector_dispose(&function->records);
    mmay_vector_dispose(&function->enum_arguments);
    mmay_vector_dispose(&function->enum_constructions);
    mmay_vector_dispose(&function->enum_payloads);
    mmay_vector_dispose(&function->enum_variants);
    mmay_vector_dispose(&function->enums);
    mmay_vector_dispose(&function->blocks);
    mmay_vector_dispose(&function->register_types);
    memset(function, 0, sizeof(*function));
}
bool mmay_mir_add_register(
    MarimayMirFunction *function,
    MarimayTypeId type,
    MarimayMirRegister *result
) {
    if (function == NULL || !function->initialized || result == NULL
        || type == MMAY_TYPE_INVALID
        || function->register_types.length >= function->options.max_registers
        || function->register_types.length >= UINT32_MAX) {
        return false;
    }
    if (!mmay_vector_push(&function->register_types, &type)) {
        return false;
    }
    *result = (MarimayMirRegister)(function->register_types.length - 1);
    return true;
}
bool mmay_mir_add_block(MarimayMirFunction *function, MarimayMirBlockId *result) {
    MarimayMirBlock block;
    if (function == NULL || !function->initialized || result == NULL
        || function->blocks.length >= function->options.max_blocks
        || function->blocks.length >= UINT32_MAX) {
        return false;
    }
    memset(&block, 0, sizeof(block));
    if (!mmay_vector_init(
            &block.instructions,
            function->allocator,
            sizeof(MarimayMirInstruction)
        )) {
        return false;
    }
    block.allocator = function->allocator;
    block.terminator.kind = MMAY_MIR_TERMINATOR_UNSET;
    block.terminator.value = MMAY_MIR_NO_REGISTER;
    block.terminator.call = MMAY_MIR_NO_CALL;
    block.terminator.target = MMAY_MIR_NO_BLOCK;
    block.terminator.otherwise = MMAY_MIR_NO_BLOCK;
    block.initialized = true;
    if (!mmay_vector_push(&function->blocks, &block)) {
        mmay_vector_dispose(&block.instructions);
        return false;
    }
    *result = (MarimayMirBlockId)(function->blocks.length - 1);
    return true;
}
bool mmay_mir_add_call(
    MarimayMirFunction *function,
    MarimaySymbolId callee,
    MarimayTypeId result_type,
    const MarimayMirCallArgument *arguments,
    size_t argument_count,
    MarimayMirCallId *result
) {
    MarimayMirCall call;
    size_t new_argument_count = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || result_type == MMAY_TYPE_INVALID
        || (arguments == NULL && argument_count != 0)
        || function->calls.length >= function->options.max_calls
        || function->calls.length >= UINT32_MAX
        || !mmay_size_add(
            function->call_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_call_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayTypeId *actual_type = mmay_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        if (arguments[index].type == MMAY_TYPE_INVALID
            || actual_type == NULL || *actual_type != arguments[index].type) {
            return false;
        }
    }
    if (!mmay_vector_reserve(
            &function->calls,
            function->calls.length + 1
        ) || !mmay_vector_reserve(
            &function->call_arguments,
            new_argument_count
        )) {
        return false;
    }
    call.callee = callee;
    call.result_type = result_type;
    call.interface_slot = UINT32_MAX;
    call.argument_start = function->call_arguments.length;
    call.argument_count = argument_count;
    if (!mmay_vector_append(
            &function->call_arguments,
            arguments,
            argument_count
        ) || !mmay_vector_push(&function->calls, &call)) {
        function->call_arguments.length = call.argument_start;
        return false;
    }
    *result = (MarimayMirCallId)(function->calls.length - 1);
    return true;
}

bool mmay_mir_set_call_interface_slot(
    MarimayMirFunction *function,
    MarimayMirCallId call,
    uint32_t slot
) {
    MarimayMirCall *stored = function != NULL && function->initialized
        ? mmay_vector_at(&function->calls, call) : NULL;
    if (stored == NULL) {
        return false;
    }
    stored->interface_slot = slot;
    return true;
}
bool mmay_mir_add_record(
    MarimayMirFunction *function,
    MarimayTypeId type,
    const MarimayMirRecordField *fields,
    size_t field_count,
    MarimayMirRecordId *result
) {
    size_t new_field_count = 0;
    size_t record_index = 0;
    size_t field_index = 0;
    MarimayMirRecord record;
    if (function == NULL || !function->initialized || result == NULL
        || type == MMAY_TYPE_INVALID || (fields == NULL && field_count != 0)) {
        return false;
    }
    for (field_index = 0; field_index < field_count; field_index += 1) {
        if (fields[field_index].type == MMAY_TYPE_INVALID) {
            return false;
        }
    }
    for (record_index = 0; record_index < function->records.length;
        record_index += 1) {
        const MarimayMirRecord *existing = mmay_vector_at_const(
            &function->records,
            record_index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->field_count != field_count) {
                return false;
            }
            for (field_index = 0; field_index < field_count;
                field_index += 1) {
                const MarimayMirRecordField *existing_field =
                    mmay_vector_at_const(
                        &function->record_fields,
                        existing->field_start + field_index
                    );
                if (existing_field == NULL
                    || existing_field->type != fields[field_index].type
                    || existing_field->is_mutable
                        != fields[field_index].is_mutable) {
                    return false;
                }
            }
            *result = (MarimayMirRecordId)record_index;
            return true;
        }
    }
    if (function->records.length >= function->options.max_record_layouts
        || function->records.length >= UINT32_MAX
        || !mmay_size_add(
            function->record_fields.length,
            field_count,
            &new_field_count
        ) || new_field_count > function->options.max_record_fields) {
        return false;
    }
    record.type = type;
    record.field_start = function->record_fields.length;
    record.field_count = field_count;
    if (!mmay_vector_reserve(
            &function->records,
            function->records.length + 1
        ) || !mmay_vector_reserve(
            &function->record_fields,
            new_field_count
        ) || !mmay_vector_append(
            &function->record_fields,
            fields,
            field_count
        )) {
        return false;
    }
    if (!mmay_vector_push(&function->records, &record)) {
        function->record_fields.length = record.field_start;
        return false;
    }
    *result = (MarimayMirRecordId)(function->records.length - 1);
    return true;
}
bool mmay_mir_add_record_construction(
    MarimayMirFunction *function,
    MarimayMirRecordId record_id,
    const MarimayMirRecordArgument *arguments,
    size_t argument_count,
    MarimayMirRecordConstructionId *result
) {
    const MarimayMirRecord *record = NULL;
    size_t new_argument_count = 0;
    size_t index = 0;
    MarimayMirRecordConstruction construction;
    if (function == NULL || !function->initialized || result == NULL
        || (arguments == NULL && argument_count != 0)
        || function->record_constructions.length
            >= function->options.max_record_constructions
        || function->record_constructions.length >= UINT32_MAX
        || !mmay_size_add(
            function->record_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_record_arguments) {
        return false;
    }
    record = mmay_vector_at_const(&function->records, record_id);
    if (record == NULL || record->field_count != argument_count) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayTypeId *actual_type = mmay_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        const MarimayMirRecordField *field = mmay_vector_at_const(
            &function->record_fields,
            record->field_start + index
        );
        if (arguments[index].type == MMAY_TYPE_INVALID || actual_type == NULL
            || field == NULL || *actual_type != arguments[index].type
            || field->type != arguments[index].type) {
            return false;
        }
    }
    construction.record = record_id;
    construction.argument_start = function->record_arguments.length;
    construction.argument_count = argument_count;
    if (!mmay_vector_reserve(
            &function->record_constructions,
            function->record_constructions.length + 1
        ) || !mmay_vector_reserve(
            &function->record_arguments,
            new_argument_count
        ) || !mmay_vector_append(
            &function->record_arguments,
            arguments,
            argument_count
        )) {
        return false;
    }
    if (!mmay_vector_push(
            &function->record_constructions,
            &construction
        )) {
        function->record_arguments.length = construction.argument_start;
        return false;
    }
    *result = (MarimayMirRecordConstructionId)(
        function->record_constructions.length - 1
    );
    return true;
}
bool mmay_mir_add_enum_with_symbol(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayTypeId type,
    const MarimayMirEnumVariant *variants,
    size_t variant_count,
    const MarimayMirEnumPayload *payloads,
    size_t payload_count,
    MarimayMirEnumId *result
) {
    size_t enum_index = 0;
    size_t variant_index = 0;
    size_t payload_index = 0;
    size_t new_variant_count = 0;
    size_t new_payload_count = 0;
    size_t payload_cursor = 0;
    MarimayMirEnum enum_layout;
    if (function == NULL || !function->initialized || result == NULL
        || type == MMAY_TYPE_INVALID
        || (variants == NULL && variant_count != 0)
        || (payloads == NULL && payload_count != 0)) {
        return false;
    }
    for (variant_index = 0; variant_index < variant_count;
        variant_index += 1) {
        size_t payload_end = 0;
        if (variants[variant_index].kind < MMAY_MIR_ENUM_VARIANT_UNIT
            || variants[variant_index].kind > MMAY_MIR_ENUM_VARIANT_NAMED
            || variants[variant_index].payload_start != payload_cursor
            || !mmay_size_add(
                variants[variant_index].payload_start,
                variants[variant_index].payload_count,
                &payload_end
            ) || payload_end > payload_count
            || (variants[variant_index].kind == MMAY_MIR_ENUM_VARIANT_UNIT
                && variants[variant_index].payload_count != 0)) {
            return false;
        }
        payload_cursor = payload_end;
    }
    if (payload_cursor != payload_count) {
        return false;
    }
    for (payload_index = 0; payload_index < payload_count;
        payload_index += 1) {
        if (payloads[payload_index].type == MMAY_TYPE_INVALID) {
            return false;
        }
    }
    for (enum_index = 0; enum_index < function->enums.length;
        enum_index += 1) {
        const MarimayMirEnum *existing = mmay_vector_at_const(
            &function->enums,
            enum_index
        );
        bool equal = existing != NULL && existing->type == type
            && memcmp(existing->symbol.bytes, symbol.bytes,
                sizeof(symbol.bytes)) == 0
            && existing->variant_count == variant_count
            && existing->payload_count == payload_count;
        if (existing == NULL || existing->type != type) {
            continue;
        }
        for (variant_index = 0; equal && variant_index < variant_count;
            variant_index += 1) {
            const MarimayMirEnumVariant *existing_variant = mmay_vector_at_const(
                &function->enum_variants,
                existing->variant_start + variant_index
            );
            if (existing_variant == NULL
                || existing_variant->kind != variants[variant_index].kind
                || existing_variant->payload_count
                    != variants[variant_index].payload_count
                || existing_variant->payload_start
                    != existing->payload_start
                        + variants[variant_index].payload_start) {
                equal = false;
            }
        }
        for (payload_index = 0; equal && payload_index < payload_count;
            payload_index += 1) {
            const MarimayMirEnumPayload *existing_payload = mmay_vector_at_const(
                &function->enum_payloads,
                existing->payload_start + payload_index
            );
            if (existing_payload == NULL
                || existing_payload->type != payloads[payload_index].type) {
                equal = false;
            }
        }
        if (!equal) {
            return false;
        }
        *result = (MarimayMirEnumId)enum_index;
        return true;
    }
    if (function->enums.length >= function->options.max_enum_layouts
        || function->enums.length >= UINT32_MAX
        || !mmay_size_add(
            function->enum_variants.length,
            variant_count,
            &new_variant_count
        ) || new_variant_count > function->options.max_enum_variants
        || !mmay_size_add(
            function->enum_payloads.length,
            payload_count,
            &new_payload_count
        ) || new_payload_count > function->options.max_enum_payloads
        || !mmay_vector_reserve(
            &function->enums,
            function->enums.length + 1
        ) || !mmay_vector_reserve(
            &function->enum_variants,
            new_variant_count
        ) || !mmay_vector_reserve(
            &function->enum_payloads,
            new_payload_count
        )) {
        return false;
    }
    enum_layout.symbol = symbol;
    enum_layout.type = type;
    enum_layout.variant_start = function->enum_variants.length;
    enum_layout.variant_count = variant_count;
    enum_layout.payload_start = function->enum_payloads.length;
    enum_layout.payload_count = payload_count;
    if (!mmay_vector_append(
            &function->enum_payloads,
            payloads,
            payload_count
        )) {
        return false;
    }
    for (variant_index = 0; variant_index < variant_count;
        variant_index += 1) {
        MarimayMirEnumVariant stored = variants[variant_index];
        stored.payload_start += enum_layout.payload_start;
        if (!mmay_vector_push(&function->enum_variants, &stored)) {
            function->enum_variants.length = enum_layout.variant_start;
            function->enum_payloads.length = enum_layout.payload_start;
            return false;
        }
    }
    if (!mmay_vector_push(&function->enums, &enum_layout)) {
        function->enum_variants.length = enum_layout.variant_start;
        function->enum_payloads.length = enum_layout.payload_start;
        return false;
    }
    *result = (MarimayMirEnumId)(function->enums.length - 1);
    return true;
}

bool mmay_mir_add_enum(
    MarimayMirFunction *function,
    MarimayTypeId type,
    const MarimayMirEnumVariant *variants,
    size_t variant_count,
    const MarimayMirEnumPayload *payloads,
    size_t payload_count,
    MarimayMirEnumId *result
) {
    MarimaySymbolId symbol;
    memset(&symbol, 0, sizeof(symbol));
    memcpy(symbol.bytes, &type, sizeof(type));
    symbol.bytes[sizeof(type)] = 0x45;
    return mmay_mir_add_enum_with_symbol(
        function,
        symbol,
        type,
        variants,
        variant_count,
        payloads,
        payload_count,
        result
    );
}
bool mmay_mir_add_enum_construction(
    MarimayMirFunction *function,
    MarimayMirEnumId enum_id,
    size_t variant,
    const MarimayMirEnumArgument *arguments,
    size_t argument_count,
    MarimayMirEnumConstructionId *result
) {
    const MarimayMirEnum *enum_layout = NULL;
    const MarimayMirEnumVariant *variant_layout = NULL;
    size_t new_argument_count = 0;
    size_t index = 0;
    MarimayMirEnumConstruction construction;
    if (function == NULL || !function->initialized || result == NULL
        || (arguments == NULL && argument_count != 0)
        || function->enum_constructions.length
            >= function->options.max_enum_constructions
        || function->enum_constructions.length >= UINT32_MAX
        || !mmay_size_add(
            function->enum_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_enum_arguments) {
        return false;
    }
    enum_layout = mmay_vector_at_const(&function->enums, enum_id);
    variant_layout = enum_layout != NULL && variant < enum_layout->variant_count
        ? mmay_vector_at_const(
            &function->enum_variants,
            enum_layout->variant_start + variant
        )
        : NULL;
    if (variant_layout == NULL
        || variant_layout->payload_count != argument_count) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayTypeId *actual_type = mmay_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        const MarimayMirEnumPayload *payload = mmay_vector_at_const(
            &function->enum_payloads,
            variant_layout->payload_start + index
        );
        if (arguments[index].type == MMAY_TYPE_INVALID || actual_type == NULL
            || payload == NULL || payload->type == MMAY_TYPE_NEVER
            || *actual_type != arguments[index].type
            || payload->type != arguments[index].type) {
            return false;
        }
    }
    construction.enum_id = enum_id;
    construction.variant = variant;
    construction.argument_start = function->enum_arguments.length;
    construction.argument_count = argument_count;
    if (!mmay_vector_reserve(
            &function->enum_constructions,
            function->enum_constructions.length + 1
        ) || !mmay_vector_reserve(
            &function->enum_arguments,
            new_argument_count
        ) || !mmay_vector_append(
            &function->enum_arguments,
            arguments,
            argument_count
        )) {
        return false;
    }
    if (!mmay_vector_push(
            &function->enum_constructions,
            &construction
        )) {
        function->enum_arguments.length = construction.argument_start;
        return false;
    }
    *result = (MarimayMirEnumConstructionId)(
        function->enum_constructions.length - 1
    );
    return true;
}
bool mmay_mir_add_string_constant(
    MarimayMirFunction *function,
    const unsigned char *bytes,
    size_t length,
    MarimayMirStringId *result
) {
    MarimayMirStringConstant constant;
    size_t new_total = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || !mmay_utf8_validate((const char *)bytes, length, NULL)
        || function->string_constants.length
            >= function->options.max_string_constants
        || function->string_constants.length >= UINT32_MAX
        || !mmay_size_add(function->string_bytes, length, &new_total)
        || new_total > function->options.max_string_bytes) {
        return false;
    }
    memset(&constant, 0, sizeof(constant));
    if (!mmay_owned_bytes_copy(
            &constant.value,
            function->allocator,
            bytes,
            length
        )) {
        return false;
    }
    if (!mmay_vector_push(&function->string_constants, &constant)) {
        mmay_owned_bytes_dispose(&constant.value);
        return false;
    }
    function->string_bytes = new_total;
    *result = (MarimayMirStringId)(function->string_constants.length - 1);
    return true;
}
bool mmay_mir_add_array(
    MarimayMirFunction *function,
    MarimayTypeId type,
    MarimayTypeId element_type,
    MarimayMirArrayId *result
) {
    size_t index = 0;
    MarimayMirArray array;
    if (function == NULL || !function->initialized || result == NULL
        || type < MMAY_TYPE_FIRST_DYNAMIC
        || element_type == MMAY_TYPE_INVALID) {
        return false;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *existing = mmay_vector_at_const(
            &function->arrays,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->element_type != element_type) {
                return false;
            }
            *result = (MarimayMirArrayId)index;
            return true;
        }
    }
    if (function->arrays.length >= function->options.max_array_layouts
        || function->arrays.length >= UINT32_MAX) {
        return false;
    }
    array.type = type;
    array.element_type = element_type;
    if (!mmay_vector_push(&function->arrays, &array)) {
        return false;
    }
    *result = (MarimayMirArrayId)(function->arrays.length - 1);
    return true;
}
bool mmay_mir_add_array_construction(
    MarimayMirFunction *function,
    MarimayMirArrayId array_id,
    const MarimayMirArrayArgument *arguments,
    size_t argument_count,
    MarimayMirArrayConstructionId *result
) {
    const MarimayMirArray *array = NULL;
    MarimayMirArrayConstruction construction;
    size_t new_argument_count = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (arguments == NULL && argument_count != 0)
        || function->array_constructions.length
            >= function->options.max_array_constructions
        || function->array_constructions.length >= UINT32_MAX
        || !mmay_size_add(
            function->array_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_array_arguments) {
        return false;
    }
    array = mmay_vector_at_const(&function->arrays, array_id);
    if (array == NULL) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayTypeId *actual = mmay_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        if (actual == NULL || arguments[index].type != array->element_type
            || *actual != arguments[index].type) {
            return false;
        }
    }
    construction.array = array_id;
    construction.argument_start = function->array_arguments.length;
    construction.argument_count = argument_count;
    if (!mmay_vector_reserve(
            &function->array_constructions,
            function->array_constructions.length + 1
        ) || !mmay_vector_reserve(
            &function->array_arguments,
            new_argument_count
        ) || !mmay_vector_append(
            &function->array_arguments,
            arguments,
            argument_count
        )) {
        return false;
    }
    if (!mmay_vector_push(&function->array_constructions, &construction)) {
        function->array_arguments.length = construction.argument_start;
        return false;
    }
    *result = (MarimayMirArrayConstructionId)(
        function->array_constructions.length - 1
    );
    return true;
}

bool mmay_mir_append_instruction(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    const MarimayMirInstruction *instruction
) {
    MarimayMirBlock *target = NULL;
    if (function == NULL || !function->initialized || instruction == NULL
        || block >= function->blocks.length
        || function->instruction_count >= function->options.max_instructions) {
        return false;
    }
    target = mmay_vector_at(&function->blocks, block);
    if (target == NULL || !target->initialized
        || target->terminator.kind != MMAY_MIR_TERMINATOR_UNSET
        || !mmay_vector_push(&target->instructions, instruction)) {
        return false;
    }
    function->instruction_count += 1;
    return true;
}
bool mmay_mir_set_terminator(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    MarimayMirTerminator terminator
) {
    MarimayMirBlock *target = NULL;
    if (function == NULL || !function->initialized
        || block >= function->blocks.length
        || terminator.kind == MMAY_MIR_TERMINATOR_UNSET) {
        return false;
    }
    target = mmay_vector_at(&function->blocks, block);
    if (target == NULL || !target->initialized
        || target->terminator.kind != MMAY_MIR_TERMINATOR_UNSET) {
        return false;
    }
    target->terminator = terminator;
    return true;
}
MarimayTypeId mmay_mir_register_type(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    const MarimayTypeId *type = register_id != MMAY_MIR_NO_REGISTER
        ? mmay_vector_at_const(&function->register_types, register_id)
        : NULL;
    return type != NULL ? *type : MMAY_TYPE_INVALID;
}
bool mmay_mir_register_is(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    MarimayTypeId type
) {
    return mmay_mir_register_type(function, register_id) == type;
}
const MarimayMirCall *mmay_mir_call(
    const MarimayMirFunction *function,
    MarimayMirCallId call_id
) {
    return call_id != MMAY_MIR_NO_CALL
        ? mmay_vector_at_const(&function->calls, call_id)
        : NULL;
}
const MarimayMirCallArgument *mmay_mir_call_argument(
    const MarimayMirFunction *function,
    size_t index
) {
    return mmay_vector_at_const(&function->call_arguments, index);
}
const MarimayMirRecord *mmay_mir_record(
    const MarimayMirFunction *function,
    MarimayMirRecordId record_id
) {
    return record_id != MMAY_MIR_NO_RECORD
        ? mmay_vector_at_const(&function->records, record_id)
        : NULL;
}
const MarimayMirRecordField *mmay_mir_record_field(
    const MarimayMirFunction *function,
    const MarimayMirRecord *record,
    size_t field
) {
    return record != NULL && field < record->field_count
        ? mmay_vector_at_const(
            &function->record_fields,
            record->field_start + field
        )
        : NULL;
}
const MarimayMirRecordConstruction *mmay_mir_record_construction(
    const MarimayMirFunction *function,
    MarimayMirRecordConstructionId construction_id
) {
    return construction_id != MMAY_MIR_NO_RECORD_CONSTRUCTION
        ? mmay_vector_at_const(
            &function->record_constructions,
            construction_id
        )
        : NULL;
}
const MarimayMirRecordArgument *mmay_mir_record_argument(
    const MarimayMirFunction *function,
    size_t index
) {
    return mmay_vector_at_const(&function->record_arguments, index);
}
const MarimayMirEnum *mmay_mir_enum(
    const MarimayMirFunction *function,
    MarimayMirEnumId enum_id
) {
    return enum_id != MMAY_MIR_NO_ENUM
        ? mmay_vector_at_const(&function->enums, enum_id)
        : NULL;
}
const MarimayMirEnumVariant *mmay_mir_enum_variant(
    const MarimayMirFunction *function,
    const MarimayMirEnum *enum_layout,
    size_t variant
) {
    return enum_layout != NULL && variant < enum_layout->variant_count
        ? mmay_vector_at_const(
            &function->enum_variants,
            enum_layout->variant_start + variant
        )
        : NULL;
}
const MarimayMirEnumPayload *mmay_mir_enum_payload(
    const MarimayMirFunction *function,
    const MarimayMirEnumVariant *variant,
    size_t payload
) {
    return variant != NULL && payload < variant->payload_count
        ? mmay_vector_at_const(
            &function->enum_payloads,
            variant->payload_start + payload
        )
        : NULL;
}
const MarimayMirEnumConstruction *mmay_mir_enum_construction(
    const MarimayMirFunction *function,
    MarimayMirEnumConstructionId construction_id
) {
    return construction_id != MMAY_MIR_NO_ENUM_CONSTRUCTION
        ? mmay_vector_at_const(
            &function->enum_constructions,
            construction_id
        )
        : NULL;
}
const MarimayMirEnumArgument *mmay_mir_enum_argument(
    const MarimayMirFunction *function,
    size_t index
) {
    return mmay_vector_at_const(&function->enum_arguments, index);
}
const MarimayMirStringConstant *mmay_mir_string_constant(
    const MarimayMirFunction *function,
    MarimayMirStringId string_id
) {
    return string_id != MMAY_MIR_NO_STRING
        ? mmay_vector_at_const(&function->string_constants, string_id)
        : NULL;
}
const MarimayMirArray *mmay_mir_array(
    const MarimayMirFunction *function,
    MarimayMirArrayId array_id
) {
    return array_id != MMAY_MIR_NO_ARRAY
        ? mmay_vector_at_const(&function->arrays, array_id)
        : NULL;
}
const MarimayMirArrayConstruction *mmay_mir_array_construction(
    const MarimayMirFunction *function,
    MarimayMirArrayConstructionId construction_id
) {
    return construction_id != MMAY_MIR_NO_ARRAY_CONSTRUCTION
        ? mmay_vector_at_const(
            &function->array_constructions,
            construction_id
        )
        : NULL;
}
const MarimayMirArrayArgument *mmay_mir_array_argument(
    const MarimayMirFunction *function,
    size_t index
) {
    return mmay_vector_at_const(&function->array_arguments, index);
}
