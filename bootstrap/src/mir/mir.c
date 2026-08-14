#include "mir_internal.h"
#include "ruda/base/utf8.h"
#include <string.h>
bool ruda_mir_reference_is_invalid(RudaMirReference reference) {
    return reference.execution_provenance == UINT64_MAX
        && reference.slot == UINT32_MAX;
}
bool ruda_mir_reference_equal(
    RudaMirReference left,
    RudaMirReference right
) {
    return left.execution_provenance == right.execution_provenance
        && left.slot == right.slot;
}
RudaMirFunctionOptions ruda_mir_function_default_options(void) {
    RudaMirFunctionOptions options;
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
    options.max_interface_layouts = 100000;
    options.max_interface_constructions = 100000;
    options.max_interface_methods = 1000000;
    options.max_verify_work = 100000000;
    return options;
}
bool ruda_mir_function_init(
    RudaMirFunction *function,
    RudaAllocator allocator,
    RudaSymbolId symbol,
    const RudaTypeId *parameter_types,
    size_t parameter_count,
    RudaTypeId result_type,
    RudaMirFunctionOptions options
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
    bool interfaces_initialized = false;
    bool interface_constructions_initialized = false;
    bool interface_methods_initialized = false;
    if (function == NULL || allocator.allocate == NULL
        || allocator.deallocate == NULL
        || (parameter_types == NULL && parameter_count != 0)
        || parameter_count > options.max_registers
        || parameter_count > UINT32_MAX
        || result_type == RUDA_TYPE_INVALID || options.max_registers == 0
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
        || options.max_interface_layouts == 0
        || options.max_interface_layouts > UINT32_MAX
        || options.max_interface_constructions == 0
        || options.max_interface_constructions > UINT32_MAX
        || options.max_interface_methods == 0
        || options.max_verify_work == 0) {
        return false;
    }
    for (index = 0; index < parameter_count; index += 1) {
        if (parameter_types[index] == RUDA_TYPE_INVALID) {
            return false;
        }
    }
    memset(function, 0, sizeof(*function));
    registers_initialized = ruda_vector_init(
        &function->register_types,
        allocator,
        sizeof(RudaTypeId)
    );
    blocks_initialized = ruda_vector_init(
        &function->blocks,
        allocator,
        sizeof(RudaMirBlock)
    );
    calls_initialized = ruda_vector_init(
        &function->calls,
        allocator,
        sizeof(RudaMirCall)
    );
    call_arguments_initialized = ruda_vector_init(
        &function->call_arguments,
        allocator,
        sizeof(RudaMirCallArgument)
    );
    records_initialized = ruda_vector_init(
        &function->records,
        allocator,
        sizeof(RudaMirRecord)
    );
    record_fields_initialized = ruda_vector_init(
        &function->record_fields,
        allocator,
        sizeof(RudaMirRecordField)
    );
    record_constructions_initialized = ruda_vector_init(
        &function->record_constructions,
        allocator,
        sizeof(RudaMirRecordConstruction)
    );
    record_arguments_initialized = ruda_vector_init(
        &function->record_arguments,
        allocator,
        sizeof(RudaMirRecordArgument)
    );
    enums_initialized = ruda_vector_init(
        &function->enums,
        allocator,
        sizeof(RudaMirEnum)
    );
    enum_variants_initialized = ruda_vector_init(
        &function->enum_variants,
        allocator,
        sizeof(RudaMirEnumVariant)
    );
    enum_payloads_initialized = ruda_vector_init(
        &function->enum_payloads,
        allocator,
        sizeof(RudaMirEnumPayload)
    );
    enum_constructions_initialized = ruda_vector_init(
        &function->enum_constructions,
        allocator,
        sizeof(RudaMirEnumConstruction)
    );
    enum_arguments_initialized = ruda_vector_init(
        &function->enum_arguments,
        allocator,
        sizeof(RudaMirEnumArgument)
    );
    string_constants_initialized = ruda_vector_init(
        &function->string_constants,
        allocator,
        sizeof(RudaMirStringConstant)
    );
    arrays_initialized = ruda_vector_init(
        &function->arrays,
        allocator,
        sizeof(RudaMirArray)
    );
    array_constructions_initialized = ruda_vector_init(
        &function->array_constructions,
        allocator,
        sizeof(RudaMirArrayConstruction)
    );
    array_arguments_initialized = ruda_vector_init(
        &function->array_arguments,
        allocator,
        sizeof(RudaMirArrayArgument)
    );
    interfaces_initialized = ruda_vector_init(
        &function->interfaces, allocator, sizeof(RudaMirInterface)
    );
    interface_constructions_initialized = ruda_vector_init(
        &function->interface_constructions, allocator,
        sizeof(RudaMirInterfaceConstruction)
    );
    interface_methods_initialized = ruda_vector_init(
        &function->interface_methods, allocator, sizeof(RudaSymbolId)
    );
    if (!registers_initialized || !blocks_initialized || !calls_initialized
        || !call_arguments_initialized || !records_initialized
        || !record_fields_initialized || !record_constructions_initialized
        || !record_arguments_initialized || !enums_initialized
        || !enum_variants_initialized || !enum_payloads_initialized
        || !enum_constructions_initialized || !enum_arguments_initialized
        || !string_constants_initialized || !arrays_initialized
        || !array_constructions_initialized || !array_arguments_initialized
        || !interfaces_initialized || !interface_constructions_initialized
        || !interface_methods_initialized
        || (parameter_count != 0 && !ruda_vector_append(
            &function->register_types,
            parameter_types,
            parameter_count
        ))) {
        if (interface_methods_initialized) {
            ruda_vector_dispose(&function->interface_methods);
        }
        if (interface_constructions_initialized) {
            ruda_vector_dispose(&function->interface_constructions);
        }
        if (interfaces_initialized) {
            ruda_vector_dispose(&function->interfaces);
        }
        if (array_arguments_initialized) {
            ruda_vector_dispose(&function->array_arguments);
        }
        if (array_constructions_initialized) {
            ruda_vector_dispose(&function->array_constructions);
        }
        if (arrays_initialized) {
            ruda_vector_dispose(&function->arrays);
        }
        if (string_constants_initialized) {
            ruda_vector_dispose(&function->string_constants);
        }
        if (enum_arguments_initialized) {
            ruda_vector_dispose(&function->enum_arguments);
        }
        if (enum_constructions_initialized) {
            ruda_vector_dispose(&function->enum_constructions);
        }
        if (enum_payloads_initialized) {
            ruda_vector_dispose(&function->enum_payloads);
        }
        if (enum_variants_initialized) {
            ruda_vector_dispose(&function->enum_variants);
        }
        if (enums_initialized) {
            ruda_vector_dispose(&function->enums);
        }
        if (record_arguments_initialized) {
            ruda_vector_dispose(&function->record_arguments);
        }
        if (record_constructions_initialized) {
            ruda_vector_dispose(&function->record_constructions);
        }
        if (record_fields_initialized) {
            ruda_vector_dispose(&function->record_fields);
        }
        if (records_initialized) {
            ruda_vector_dispose(&function->records);
        }
        if (call_arguments_initialized) {
            ruda_vector_dispose(&function->call_arguments);
        }
        if (calls_initialized) {
            ruda_vector_dispose(&function->calls);
        }
        if (blocks_initialized) {
            ruda_vector_dispose(&function->blocks);
        }
        if (registers_initialized) {
            ruda_vector_dispose(&function->register_types);
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
void ruda_mir_function_dispose(RudaMirFunction *function) {
    size_t index = 0;
    if (function == NULL || !function->initialized) {
        return;
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        RudaMirBlock *block = ruda_vector_at(&function->blocks, index);
        if (block != NULL && block->initialized) {
            ruda_vector_dispose(&block->instructions);
            memset(block, 0, sizeof(*block));
        }
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        RudaMirStringConstant *constant = ruda_vector_at(
            &function->string_constants,
            index
        );
        if (constant != NULL) {
            ruda_owned_bytes_dispose(&constant->value);
        }
    }
    ruda_vector_dispose(&function->string_constants);
    ruda_vector_dispose(&function->interface_methods);
    ruda_vector_dispose(&function->interface_constructions);
    ruda_vector_dispose(&function->interfaces);
    ruda_vector_dispose(&function->array_arguments);
    ruda_vector_dispose(&function->array_constructions);
    ruda_vector_dispose(&function->arrays);
    ruda_vector_dispose(&function->call_arguments);
    ruda_vector_dispose(&function->calls);
    ruda_vector_dispose(&function->record_arguments);
    ruda_vector_dispose(&function->record_constructions);
    ruda_vector_dispose(&function->record_fields);
    ruda_vector_dispose(&function->records);
    ruda_vector_dispose(&function->enum_arguments);
    ruda_vector_dispose(&function->enum_constructions);
    ruda_vector_dispose(&function->enum_payloads);
    ruda_vector_dispose(&function->enum_variants);
    ruda_vector_dispose(&function->enums);
    ruda_vector_dispose(&function->blocks);
    ruda_vector_dispose(&function->register_types);
    memset(function, 0, sizeof(*function));
}
bool ruda_mir_add_register(
    RudaMirFunction *function,
    RudaTypeId type,
    RudaMirRegister *result
) {
    if (function == NULL || !function->initialized || result == NULL
        || type == RUDA_TYPE_INVALID
        || function->register_types.length >= function->options.max_registers
        || function->register_types.length >= UINT32_MAX) {
        return false;
    }
    if (!ruda_vector_push(&function->register_types, &type)) {
        return false;
    }
    *result = (RudaMirRegister)(function->register_types.length - 1);
    return true;
}
bool ruda_mir_add_block(RudaMirFunction *function, RudaMirBlockId *result) {
    RudaMirBlock block;
    if (function == NULL || !function->initialized || result == NULL
        || function->blocks.length >= function->options.max_blocks
        || function->blocks.length >= UINT32_MAX) {
        return false;
    }
    memset(&block, 0, sizeof(block));
    if (!ruda_vector_init(
            &block.instructions,
            function->allocator,
            sizeof(RudaMirInstruction)
        )) {
        return false;
    }
    block.allocator = function->allocator;
    block.terminator.kind = RUDA_MIR_TERMINATOR_UNSET;
    block.terminator.value = RUDA_MIR_NO_REGISTER;
    block.terminator.call = RUDA_MIR_NO_CALL;
    block.terminator.target = RUDA_MIR_NO_BLOCK;
    block.terminator.otherwise = RUDA_MIR_NO_BLOCK;
    block.initialized = true;
    if (!ruda_vector_push(&function->blocks, &block)) {
        ruda_vector_dispose(&block.instructions);
        return false;
    }
    *result = (RudaMirBlockId)(function->blocks.length - 1);
    return true;
}
bool ruda_mir_add_call(
    RudaMirFunction *function,
    RudaSymbolId callee,
    RudaTypeId result_type,
    const RudaMirCallArgument *arguments,
    size_t argument_count,
    RudaMirCallId *result
) {
    RudaMirCall call;
    size_t new_argument_count = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || result_type == RUDA_TYPE_INVALID
        || (arguments == NULL && argument_count != 0)
        || function->calls.length >= function->options.max_calls
        || function->calls.length >= UINT32_MAX
        || !ruda_size_add(
            function->call_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_call_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaTypeId *actual_type = ruda_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        if (arguments[index].type == RUDA_TYPE_INVALID
            || actual_type == NULL || *actual_type != arguments[index].type) {
            return false;
        }
    }
    if (!ruda_vector_reserve(
            &function->calls,
            function->calls.length + 1
        ) || !ruda_vector_reserve(
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
    if (!ruda_vector_append(
            &function->call_arguments,
            arguments,
            argument_count
        ) || !ruda_vector_push(&function->calls, &call)) {
        function->call_arguments.length = call.argument_start;
        return false;
    }
    *result = (RudaMirCallId)(function->calls.length - 1);
    return true;
}

bool ruda_mir_set_call_interface_slot(
    RudaMirFunction *function,
    RudaMirCallId call,
    uint32_t slot
) {
    RudaMirCall *stored = function != NULL && function->initialized
        ? ruda_vector_at(&function->calls, call) : NULL;
    if (stored == NULL) {
        return false;
    }
    stored->interface_slot = slot;
    return true;
}
bool ruda_mir_add_record(
    RudaMirFunction *function,
    RudaTypeId type,
    const RudaMirRecordField *fields,
    size_t field_count,
    RudaMirRecordId *result
) {
    size_t new_field_count = 0;
    size_t record_index = 0;
    size_t field_index = 0;
    RudaMirRecord record;
    if (function == NULL || !function->initialized || result == NULL
        || type == RUDA_TYPE_INVALID || (fields == NULL && field_count != 0)) {
        return false;
    }
    for (field_index = 0; field_index < field_count; field_index += 1) {
        if (fields[field_index].type == RUDA_TYPE_INVALID) {
            return false;
        }
    }
    for (record_index = 0; record_index < function->records.length;
        record_index += 1) {
        const RudaMirRecord *existing = ruda_vector_at_const(
            &function->records,
            record_index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->field_count != field_count) {
                return false;
            }
            for (field_index = 0; field_index < field_count;
                field_index += 1) {
                const RudaMirRecordField *existing_field =
                    ruda_vector_at_const(
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
            *result = (RudaMirRecordId)record_index;
            return true;
        }
    }
    if (function->records.length >= function->options.max_record_layouts
        || function->records.length >= UINT32_MAX
        || !ruda_size_add(
            function->record_fields.length,
            field_count,
            &new_field_count
        ) || new_field_count > function->options.max_record_fields) {
        return false;
    }
    record.type = type;
    record.field_start = function->record_fields.length;
    record.field_count = field_count;
    if (!ruda_vector_reserve(
            &function->records,
            function->records.length + 1
        ) || !ruda_vector_reserve(
            &function->record_fields,
            new_field_count
        ) || !ruda_vector_append(
            &function->record_fields,
            fields,
            field_count
        )) {
        return false;
    }
    if (!ruda_vector_push(&function->records, &record)) {
        function->record_fields.length = record.field_start;
        return false;
    }
    *result = (RudaMirRecordId)(function->records.length - 1);
    return true;
}
bool ruda_mir_add_record_construction(
    RudaMirFunction *function,
    RudaMirRecordId record_id,
    const RudaMirRecordArgument *arguments,
    size_t argument_count,
    RudaMirRecordConstructionId *result
) {
    const RudaMirRecord *record = NULL;
    size_t new_argument_count = 0;
    size_t index = 0;
    RudaMirRecordConstruction construction;
    if (function == NULL || !function->initialized || result == NULL
        || (arguments == NULL && argument_count != 0)
        || function->record_constructions.length
            >= function->options.max_record_constructions
        || function->record_constructions.length >= UINT32_MAX
        || !ruda_size_add(
            function->record_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_record_arguments) {
        return false;
    }
    record = ruda_vector_at_const(&function->records, record_id);
    if (record == NULL || record->field_count != argument_count) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaTypeId *actual_type = ruda_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        const RudaMirRecordField *field = ruda_vector_at_const(
            &function->record_fields,
            record->field_start + index
        );
        if (arguments[index].type == RUDA_TYPE_INVALID || actual_type == NULL
            || field == NULL || *actual_type != arguments[index].type
            || field->type != arguments[index].type) {
            return false;
        }
    }
    construction.record = record_id;
    construction.argument_start = function->record_arguments.length;
    construction.argument_count = argument_count;
    if (!ruda_vector_reserve(
            &function->record_constructions,
            function->record_constructions.length + 1
        ) || !ruda_vector_reserve(
            &function->record_arguments,
            new_argument_count
        ) || !ruda_vector_append(
            &function->record_arguments,
            arguments,
            argument_count
        )) {
        return false;
    }
    if (!ruda_vector_push(
            &function->record_constructions,
            &construction
        )) {
        function->record_arguments.length = construction.argument_start;
        return false;
    }
    *result = (RudaMirRecordConstructionId)(
        function->record_constructions.length - 1
    );
    return true;
}
bool ruda_mir_add_enum(
    RudaMirFunction *function,
    RudaTypeId type,
    const RudaMirEnumVariant *variants,
    size_t variant_count,
    const RudaMirEnumPayload *payloads,
    size_t payload_count,
    RudaMirEnumId *result
) {
    size_t enum_index = 0;
    size_t variant_index = 0;
    size_t payload_index = 0;
    size_t new_variant_count = 0;
    size_t new_payload_count = 0;
    size_t payload_cursor = 0;
    RudaMirEnum enum_layout;
    if (function == NULL || !function->initialized || result == NULL
        || type == RUDA_TYPE_INVALID
        || (variants == NULL && variant_count != 0)
        || (payloads == NULL && payload_count != 0)) {
        return false;
    }
    for (variant_index = 0; variant_index < variant_count;
        variant_index += 1) {
        size_t payload_end = 0;
        if (variants[variant_index].kind < RUDA_MIR_ENUM_VARIANT_UNIT
            || variants[variant_index].kind > RUDA_MIR_ENUM_VARIANT_NAMED
            || variants[variant_index].payload_start != payload_cursor
            || !ruda_size_add(
                variants[variant_index].payload_start,
                variants[variant_index].payload_count,
                &payload_end
            ) || payload_end > payload_count
            || (variants[variant_index].kind == RUDA_MIR_ENUM_VARIANT_UNIT
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
        if (payloads[payload_index].type == RUDA_TYPE_INVALID) {
            return false;
        }
    }
    for (enum_index = 0; enum_index < function->enums.length;
        enum_index += 1) {
        const RudaMirEnum *existing = ruda_vector_at_const(
            &function->enums,
            enum_index
        );
        bool equal = existing != NULL && existing->type == type
            && existing->variant_count == variant_count
            && existing->payload_count == payload_count;
        if (existing == NULL || existing->type != type) {
            continue;
        }
        for (variant_index = 0; equal && variant_index < variant_count;
            variant_index += 1) {
            const RudaMirEnumVariant *existing_variant = ruda_vector_at_const(
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
            const RudaMirEnumPayload *existing_payload = ruda_vector_at_const(
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
        *result = (RudaMirEnumId)enum_index;
        return true;
    }
    if (function->enums.length >= function->options.max_enum_layouts
        || function->enums.length >= UINT32_MAX
        || !ruda_size_add(
            function->enum_variants.length,
            variant_count,
            &new_variant_count
        ) || new_variant_count > function->options.max_enum_variants
        || !ruda_size_add(
            function->enum_payloads.length,
            payload_count,
            &new_payload_count
        ) || new_payload_count > function->options.max_enum_payloads
        || !ruda_vector_reserve(
            &function->enums,
            function->enums.length + 1
        ) || !ruda_vector_reserve(
            &function->enum_variants,
            new_variant_count
        ) || !ruda_vector_reserve(
            &function->enum_payloads,
            new_payload_count
        )) {
        return false;
    }
    enum_layout.type = type;
    enum_layout.variant_start = function->enum_variants.length;
    enum_layout.variant_count = variant_count;
    enum_layout.payload_start = function->enum_payloads.length;
    enum_layout.payload_count = payload_count;
    if (!ruda_vector_append(
            &function->enum_payloads,
            payloads,
            payload_count
        )) {
        return false;
    }
    for (variant_index = 0; variant_index < variant_count;
        variant_index += 1) {
        RudaMirEnumVariant stored = variants[variant_index];
        stored.payload_start += enum_layout.payload_start;
        if (!ruda_vector_push(&function->enum_variants, &stored)) {
            function->enum_variants.length = enum_layout.variant_start;
            function->enum_payloads.length = enum_layout.payload_start;
            return false;
        }
    }
    if (!ruda_vector_push(&function->enums, &enum_layout)) {
        function->enum_variants.length = enum_layout.variant_start;
        function->enum_payloads.length = enum_layout.payload_start;
        return false;
    }
    *result = (RudaMirEnumId)(function->enums.length - 1);
    return true;
}
bool ruda_mir_add_enum_construction(
    RudaMirFunction *function,
    RudaMirEnumId enum_id,
    size_t variant,
    const RudaMirEnumArgument *arguments,
    size_t argument_count,
    RudaMirEnumConstructionId *result
) {
    const RudaMirEnum *enum_layout = NULL;
    const RudaMirEnumVariant *variant_layout = NULL;
    size_t new_argument_count = 0;
    size_t index = 0;
    RudaMirEnumConstruction construction;
    if (function == NULL || !function->initialized || result == NULL
        || (arguments == NULL && argument_count != 0)
        || function->enum_constructions.length
            >= function->options.max_enum_constructions
        || function->enum_constructions.length >= UINT32_MAX
        || !ruda_size_add(
            function->enum_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_enum_arguments) {
        return false;
    }
    enum_layout = ruda_vector_at_const(&function->enums, enum_id);
    variant_layout = enum_layout != NULL && variant < enum_layout->variant_count
        ? ruda_vector_at_const(
            &function->enum_variants,
            enum_layout->variant_start + variant
        )
        : NULL;
    if (variant_layout == NULL
        || variant_layout->payload_count != argument_count) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaTypeId *actual_type = ruda_vector_at_const(
            &function->register_types,
            arguments[index].register_id
        );
        const RudaMirEnumPayload *payload = ruda_vector_at_const(
            &function->enum_payloads,
            variant_layout->payload_start + index
        );
        if (arguments[index].type == RUDA_TYPE_INVALID || actual_type == NULL
            || payload == NULL || *actual_type != arguments[index].type
            || payload->type != arguments[index].type) {
            return false;
        }
    }
    construction.enum_id = enum_id;
    construction.variant = variant;
    construction.argument_start = function->enum_arguments.length;
    construction.argument_count = argument_count;
    if (!ruda_vector_reserve(
            &function->enum_constructions,
            function->enum_constructions.length + 1
        ) || !ruda_vector_reserve(
            &function->enum_arguments,
            new_argument_count
        ) || !ruda_vector_append(
            &function->enum_arguments,
            arguments,
            argument_count
        )) {
        return false;
    }
    if (!ruda_vector_push(
            &function->enum_constructions,
            &construction
        )) {
        function->enum_arguments.length = construction.argument_start;
        return false;
    }
    *result = (RudaMirEnumConstructionId)(
        function->enum_constructions.length - 1
    );
    return true;
}
bool ruda_mir_add_string_constant(
    RudaMirFunction *function,
    const unsigned char *bytes,
    size_t length,
    RudaMirStringId *result
) {
    RudaMirStringConstant constant;
    size_t new_total = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || !ruda_utf8_validate((const char *)bytes, length, NULL)
        || function->string_constants.length
            >= function->options.max_string_constants
        || function->string_constants.length >= UINT32_MAX
        || !ruda_size_add(function->string_bytes, length, &new_total)
        || new_total > function->options.max_string_bytes) {
        return false;
    }
    memset(&constant, 0, sizeof(constant));
    if (!ruda_owned_bytes_copy(
            &constant.value,
            function->allocator,
            bytes,
            length
        )) {
        return false;
    }
    if (!ruda_vector_push(&function->string_constants, &constant)) {
        ruda_owned_bytes_dispose(&constant.value);
        return false;
    }
    function->string_bytes = new_total;
    *result = (RudaMirStringId)(function->string_constants.length - 1);
    return true;
}
bool ruda_mir_add_array(
    RudaMirFunction *function,
    RudaTypeId type,
    RudaTypeId element_type,
    RudaMirArrayId *result
) {
    size_t index = 0;
    RudaMirArray array;
    if (function == NULL || !function->initialized || result == NULL
        || type < RUDA_TYPE_FIRST_DYNAMIC
        || element_type == RUDA_TYPE_INVALID) {
        return false;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const RudaMirArray *existing = ruda_vector_at_const(
            &function->arrays,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->element_type != element_type) {
                return false;
            }
            *result = (RudaMirArrayId)index;
            return true;
        }
    }
    if (function->arrays.length >= function->options.max_array_layouts
        || function->arrays.length >= UINT32_MAX) {
        return false;
    }
    array.type = type;
    array.element_type = element_type;
    if (!ruda_vector_push(&function->arrays, &array)) {
        return false;
    }
    *result = (RudaMirArrayId)(function->arrays.length - 1);
    return true;
}
bool ruda_mir_add_array_construction(
    RudaMirFunction *function,
    RudaMirArrayId array_id,
    const RudaMirArrayArgument *arguments,
    size_t argument_count,
    RudaMirArrayConstructionId *result
) {
    const RudaMirArray *array = NULL;
    RudaMirArrayConstruction construction;
    size_t new_argument_count = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (arguments == NULL && argument_count != 0)
        || function->array_constructions.length
            >= function->options.max_array_constructions
        || function->array_constructions.length >= UINT32_MAX
        || !ruda_size_add(
            function->array_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->options.max_array_arguments) {
        return false;
    }
    array = ruda_vector_at_const(&function->arrays, array_id);
    if (array == NULL) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaTypeId *actual = ruda_vector_at_const(
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
    if (!ruda_vector_reserve(
            &function->array_constructions,
            function->array_constructions.length + 1
        ) || !ruda_vector_reserve(
            &function->array_arguments,
            new_argument_count
        ) || !ruda_vector_append(
            &function->array_arguments,
            arguments,
            argument_count
        )) {
        return false;
    }
    if (!ruda_vector_push(&function->array_constructions, &construction)) {
        function->array_arguments.length = construction.argument_start;
        return false;
    }
    *result = (RudaMirArrayConstructionId)(
        function->array_constructions.length - 1
    );
    return true;
}

bool ruda_mir_append_instruction(
    RudaMirFunction *function,
    RudaMirBlockId block,
    const RudaMirInstruction *instruction
) {
    RudaMirBlock *target = NULL;
    if (function == NULL || !function->initialized || instruction == NULL
        || block >= function->blocks.length
        || function->instruction_count >= function->options.max_instructions) {
        return false;
    }
    target = ruda_vector_at(&function->blocks, block);
    if (target == NULL || !target->initialized
        || target->terminator.kind != RUDA_MIR_TERMINATOR_UNSET
        || !ruda_vector_push(&target->instructions, instruction)) {
        return false;
    }
    function->instruction_count += 1;
    return true;
}
bool ruda_mir_set_terminator(
    RudaMirFunction *function,
    RudaMirBlockId block,
    RudaMirTerminator terminator
) {
    RudaMirBlock *target = NULL;
    if (function == NULL || !function->initialized
        || block >= function->blocks.length
        || terminator.kind == RUDA_MIR_TERMINATOR_UNSET) {
        return false;
    }
    target = ruda_vector_at(&function->blocks, block);
    if (target == NULL || !target->initialized
        || target->terminator.kind != RUDA_MIR_TERMINATOR_UNSET) {
        return false;
    }
    target->terminator = terminator;
    return true;
}
RudaTypeId ruda_mir_register_type(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    const RudaTypeId *type = register_id != RUDA_MIR_NO_REGISTER
        ? ruda_vector_at_const(&function->register_types, register_id)
        : NULL;
    return type != NULL ? *type : RUDA_TYPE_INVALID;
}
bool ruda_mir_register_is(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    RudaTypeId type
) {
    return ruda_mir_register_type(function, register_id) == type;
}
const RudaMirCall *ruda_mir_call(
    const RudaMirFunction *function,
    RudaMirCallId call_id
) {
    return call_id != RUDA_MIR_NO_CALL
        ? ruda_vector_at_const(&function->calls, call_id)
        : NULL;
}
const RudaMirCallArgument *ruda_mir_call_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->call_arguments, index);
}
const RudaMirRecord *ruda_mir_record(
    const RudaMirFunction *function,
    RudaMirRecordId record_id
) {
    return record_id != RUDA_MIR_NO_RECORD
        ? ruda_vector_at_const(&function->records, record_id)
        : NULL;
}
const RudaMirRecordField *ruda_mir_record_field(
    const RudaMirFunction *function,
    const RudaMirRecord *record,
    size_t field
) {
    return record != NULL && field < record->field_count
        ? ruda_vector_at_const(
            &function->record_fields,
            record->field_start + field
        )
        : NULL;
}
const RudaMirRecordConstruction *ruda_mir_record_construction(
    const RudaMirFunction *function,
    RudaMirRecordConstructionId construction_id
) {
    return construction_id != RUDA_MIR_NO_RECORD_CONSTRUCTION
        ? ruda_vector_at_const(
            &function->record_constructions,
            construction_id
        )
        : NULL;
}
const RudaMirRecordArgument *ruda_mir_record_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->record_arguments, index);
}
const RudaMirEnum *ruda_mir_enum(
    const RudaMirFunction *function,
    RudaMirEnumId enum_id
) {
    return enum_id != RUDA_MIR_NO_ENUM
        ? ruda_vector_at_const(&function->enums, enum_id)
        : NULL;
}
const RudaMirEnumVariant *ruda_mir_enum_variant(
    const RudaMirFunction *function,
    const RudaMirEnum *enum_layout,
    size_t variant
) {
    return enum_layout != NULL && variant < enum_layout->variant_count
        ? ruda_vector_at_const(
            &function->enum_variants,
            enum_layout->variant_start + variant
        )
        : NULL;
}
const RudaMirEnumPayload *ruda_mir_enum_payload(
    const RudaMirFunction *function,
    const RudaMirEnumVariant *variant,
    size_t payload
) {
    return variant != NULL && payload < variant->payload_count
        ? ruda_vector_at_const(
            &function->enum_payloads,
            variant->payload_start + payload
        )
        : NULL;
}
const RudaMirEnumConstruction *ruda_mir_enum_construction(
    const RudaMirFunction *function,
    RudaMirEnumConstructionId construction_id
) {
    return construction_id != RUDA_MIR_NO_ENUM_CONSTRUCTION
        ? ruda_vector_at_const(
            &function->enum_constructions,
            construction_id
        )
        : NULL;
}
const RudaMirEnumArgument *ruda_mir_enum_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->enum_arguments, index);
}
const RudaMirStringConstant *ruda_mir_string_constant(
    const RudaMirFunction *function,
    RudaMirStringId string_id
) {
    return string_id != RUDA_MIR_NO_STRING
        ? ruda_vector_at_const(&function->string_constants, string_id)
        : NULL;
}
const RudaMirArray *ruda_mir_array(
    const RudaMirFunction *function,
    RudaMirArrayId array_id
) {
    return array_id != RUDA_MIR_NO_ARRAY
        ? ruda_vector_at_const(&function->arrays, array_id)
        : NULL;
}
const RudaMirArrayConstruction *ruda_mir_array_construction(
    const RudaMirFunction *function,
    RudaMirArrayConstructionId construction_id
) {
    return construction_id != RUDA_MIR_NO_ARRAY_CONSTRUCTION
        ? ruda_vector_at_const(
            &function->array_constructions,
            construction_id
        )
        : NULL;
}
const RudaMirArrayArgument *ruda_mir_array_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->array_arguments, index);
}
