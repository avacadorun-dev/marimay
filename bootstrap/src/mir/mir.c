#include "ruda/mir/mir.h"

#include "ruda/base/checked_i64.h"
#include "ruda/base/f64.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/map.h"
#include "ruda/base/utf8.h"

enum {
    RUDA_MIR_MAX_DATAFLOW_CELLS = 16000000
};

typedef struct RudaMirManagedRecord {
    RudaTypeId type;
    size_t field_count;
    size_t allocation_size;
    RudaMirValue fields[];
} RudaMirManagedRecord;

typedef struct RudaMirManagedEnum {
    RudaTypeId type;
    size_t variant;
    size_t variant_count;
    size_t selected_payload_count;
    size_t layout_payload_count;
    size_t allocation_size;
    RudaMirValue payload[];
} RudaMirManagedEnum;

typedef struct RudaMirManagedString {
    size_t allocation_size;
    size_t length;
    unsigned char bytes[];
} RudaMirManagedString;

static RudaMirRecordField *ruda_mir_managed_record_layout(
    RudaMirManagedRecord *record
) {
    return record != NULL ? (RudaMirRecordField *)(
        record->fields + record->field_count
    ) : NULL;
}

static const RudaMirRecordField *ruda_mir_managed_record_layout_const(
    const RudaMirManagedRecord *record
) {
    return record != NULL ? (const RudaMirRecordField *)(
        record->fields + record->field_count
    ) : NULL;
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
    options.max_verify_work = 100000000;
    return options;
}

RudaMirInterpretOptions ruda_mir_interpret_default_options(void) {
    RudaMirInterpretOptions options;
    options.max_steps = 10000000;
    options.remaining_shared_steps = NULL;
    options.call_context = NULL;
    options.call = NULL;
    options.execution = NULL;
    options.panic = NULL;
    return options;
}

RudaMirExecutionOptions ruda_mir_execution_default_options(void) {
    RudaMirExecutionOptions options;
    options.max_objects = 1000000;
    options.max_fields_per_object = 100000;
    options.max_total_fields = 10000000;
    options.max_strings = 1000000;
    options.max_total_string_bytes = 64U * 1024U * 1024U;
    return options;
}

bool ruda_mir_execution_init(
    RudaMirExecution *execution,
    RudaAllocator allocator,
    RudaMirExecutionOptions options
) {
    if (execution == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || options.max_objects == 0 || options.max_objects > UINT32_MAX
        || options.max_fields_per_object == 0
        || options.max_total_fields == 0 || options.max_strings == 0
        || options.max_strings > UINT32_MAX
        || options.max_total_string_bytes == 0) {
        return false;
    }
    memset(execution, 0, sizeof(*execution));
    if (!ruda_vector_init(
            &execution->objects,
            allocator,
            sizeof(RudaMirManagedRecord *)
        ) || !ruda_vector_init(
            &execution->enums,
            allocator,
            sizeof(RudaMirManagedEnum *)
        ) || !ruda_vector_init(
            &execution->strings,
            allocator,
            sizeof(RudaMirManagedString *)
        )) {
        ruda_vector_dispose(&execution->enums);
        ruda_vector_dispose(&execution->objects);
        return false;
    }
    execution->allocator = allocator;
    execution->options = options;
    execution->initialized = true;
    return true;
}

void ruda_mir_execution_dispose(RudaMirExecution *execution) {
    size_t index = 0;
    if (execution == NULL || !execution->initialized) {
        return;
    }
    for (index = 0; index < execution->objects.length; index += 1) {
        RudaMirManagedRecord **record = ruda_vector_at(
            &execution->objects,
            index
        );
        if (record != NULL && *record != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *record,
                (*record)->allocation_size
            );
            *record = NULL;
        }
    }
    for (index = 0; index < execution->enums.length; index += 1) {
        RudaMirManagedEnum **value = ruda_vector_at(
            &execution->enums,
            index
        );
        if (value != NULL && *value != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->strings.length; index += 1) {
        RudaMirManagedString **value = ruda_vector_at(
            &execution->strings,
            index
        );
        if (value != NULL && *value != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    ruda_vector_dispose(&execution->strings);
    ruda_vector_dispose(&execution->enums);
    ruda_vector_dispose(&execution->objects);
    memset(execution, 0, sizeof(*execution));
}

static const RudaMirManagedEnum *ruda_mir_execution_enum_value(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    RudaMirManagedEnum *const *stored = execution != NULL
            && execution->initialized
            && value.as.reference != RUDA_MIR_INVALID_REFERENCE
        ? ruda_vector_at_const(&execution->enums, value.as.reference)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}

bool ruda_mir_execution_inspect_enum(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *variant,
    size_t *payload_count
) {
    const RudaMirManagedEnum *stored = ruda_mir_execution_enum_value(
        execution,
        value
    );
    if (stored == NULL || variant == NULL || payload_count == NULL) {
        return false;
    }
    *variant = stored->variant;
    *payload_count = stored->selected_payload_count;
    return true;
}

bool ruda_mir_execution_enum_payload(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
) {
    const RudaMirManagedEnum *stored = ruda_mir_execution_enum_value(
        execution,
        value
    );
    if (stored == NULL || result == NULL
        || index >= stored->selected_payload_count) {
        return false;
    }
    *result = stored->payload[index];
    return true;
}

static const RudaMirManagedString *ruda_mir_execution_string_value(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    RudaMirManagedString *const *stored = execution != NULL
            && execution->initialized && value.type == RUDA_TYPE_STRING
            && value.as.reference != RUDA_MIR_INVALID_REFERENCE
        ? ruda_vector_at_const(&execution->strings, value.as.reference)
        : NULL;
    return stored != NULL ? *stored : NULL;
}

bool ruda_mir_execution_string_bytes(
    const RudaMirExecution *execution,
    RudaMirValue value,
    const unsigned char **bytes,
    size_t *length
) {
    const RudaMirManagedString *stored = ruda_mir_execution_string_value(
        execution,
        value
    );
    if (stored == NULL || bytes == NULL || length == NULL) {
        return false;
    }
    *bytes = stored->bytes;
    *length = stored->length;
    return true;
}

void ruda_mir_panic_dispose(RudaMirPanic *panic) {
    if (panic == NULL) {
        return;
    }
    ruda_owned_bytes_dispose(&panic->message);
    memset(panic, 0, sizeof(*panic));
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
    if (!registers_initialized || !blocks_initialized || !calls_initialized
        || !call_arguments_initialized || !records_initialized
        || !record_fields_initialized || !record_constructions_initialized
        || !record_arguments_initialized || !enums_initialized
        || !enum_variants_initialized || !enum_payloads_initialized
        || !enum_constructions_initialized || !enum_arguments_initialized
        || !string_constants_initialized
        || (parameter_count != 0 && !ruda_vector_append(
            &function->register_types,
            parameter_types,
            parameter_count
        ))) {
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

static RudaTypeId ruda_mir_register_type(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    const RudaTypeId *type = register_id != RUDA_MIR_NO_REGISTER
        ? ruda_vector_at_const(&function->register_types, register_id)
        : NULL;
    return type != NULL ? *type : RUDA_TYPE_INVALID;
}

static bool ruda_mir_register_is(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    RudaTypeId type
) {
    return ruda_mir_register_type(function, register_id) == type;
}

static const RudaMirCall *ruda_mir_call(
    const RudaMirFunction *function,
    RudaMirCallId call_id
) {
    return call_id != RUDA_MIR_NO_CALL
        ? ruda_vector_at_const(&function->calls, call_id)
        : NULL;
}

static const RudaMirCallArgument *ruda_mir_call_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->call_arguments, index);
}

static const RudaMirRecord *ruda_mir_record(
    const RudaMirFunction *function,
    RudaMirRecordId record_id
) {
    return record_id != RUDA_MIR_NO_RECORD
        ? ruda_vector_at_const(&function->records, record_id)
        : NULL;
}

static const RudaMirRecordField *ruda_mir_record_field(
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

static const RudaMirRecordConstruction *ruda_mir_record_construction(
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

static const RudaMirRecordArgument *ruda_mir_record_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->record_arguments, index);
}

static const RudaMirEnum *ruda_mir_enum(
    const RudaMirFunction *function,
    RudaMirEnumId enum_id
) {
    return enum_id != RUDA_MIR_NO_ENUM
        ? ruda_vector_at_const(&function->enums, enum_id)
        : NULL;
}

static const RudaMirEnumVariant *ruda_mir_enum_variant(
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

static const RudaMirEnumPayload *ruda_mir_enum_payload(
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

static const RudaMirEnumConstruction *ruda_mir_enum_construction(
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

static const RudaMirEnumArgument *ruda_mir_enum_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return ruda_vector_at_const(&function->enum_arguments, index);
}

static const RudaMirStringConstant *ruda_mir_string_constant(
    const RudaMirFunction *function,
    RudaMirStringId string_id
) {
    return string_id != RUDA_MIR_NO_STRING
        ? ruda_vector_at_const(&function->string_constants, string_id)
        : NULL;
}

static RudaMirVerifyStatus ruda_mir_validate_instruction_types(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    RudaTypeId target_type = ruda_mir_register_type(
        function,
        instruction->target
    );
    RudaTypeId first_type = ruda_mir_register_type(
        function,
        instruction->first
    );
    RudaTypeId second_type = ruda_mir_register_type(
        function,
        instruction->second
    );
    if (target_type == RUDA_TYPE_INVALID) {
        return RUDA_MIR_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->kind) {
        case RUDA_MIR_CONSTANT_I64:
            return target_type == RUDA_TYPE_I64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_F64:
            return target_type == RUDA_TYPE_F64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_BOOL:
            return target_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_CONSTANT_STRING:
            if (ruda_mir_string_constant(
                    function,
                    instruction->string_constant
                ) == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            return target_type == RUDA_TYPE_STRING
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_COPY:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == first_type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_ADD_CHECKED_I64:
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
        case RUDA_MIR_DIVIDE_CHECKED_I64:
        case RUDA_MIR_REMAINDER_CHECKED_I64:
        case RUDA_MIR_BITWISE_AND_I64:
        case RUDA_MIR_BITWISE_XOR_I64:
        case RUDA_MIR_BITWISE_OR_I64:
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
        case RUDA_MIR_SHIFT_RIGHT_I64:
        case RUDA_MIR_ADD_F64:
        case RUDA_MIR_SUBTRACT_F64:
        case RUDA_MIR_MULTIPLY_F64:
        case RUDA_MIR_DIVIDE_F64:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (instruction->kind == RUDA_MIR_ADD_F64
                || instruction->kind == RUDA_MIR_SUBTRACT_F64
                || instruction->kind == RUDA_MIR_MULTIPLY_F64
                || instruction->kind == RUDA_MIR_DIVIDE_F64) {
                return target_type == RUDA_TYPE_F64
                        && first_type == RUDA_TYPE_F64
                        && second_type == RUDA_TYPE_F64
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == RUDA_TYPE_I64
                    && first_type == RUDA_TYPE_I64
                    && second_type == RUDA_TYPE_I64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_NEGATE_CHECKED_I64:
        case RUDA_MIR_BITWISE_NOT_I64:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_I64
                    && first_type == RUDA_TYPE_I64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_NEGATE_F64:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_F64 && first_type == RUDA_TYPE_F64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_NOT_BOOL:
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_I64:
        case RUDA_MIR_LESS_I64:
        case RUDA_MIR_LESS_EQUAL_I64:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_I64
                    && second_type == RUDA_TYPE_I64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_F64:
        case RUDA_MIR_LESS_F64:
        case RUDA_MIR_LESS_EQUAL_F64:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_F64
                    && second_type == RUDA_TYPE_F64
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_BOOL:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_BOOL
                    && second_type == RUDA_TYPE_BOOL
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_EQUAL_STRING:
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == RUDA_TYPE_STRING
                    && second_type == RUDA_TYPE_STRING
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        case RUDA_MIR_RECORD_IDENTITY_SAME: {
            const RudaMirRecord *record = ruda_mir_record(
                function,
                instruction->record
            );
            if (record == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID
                || second_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            return target_type == RUDA_TYPE_BOOL
                    && first_type == record->type
                    && second_type == record->type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        case RUDA_MIR_RECORD_NEW: {
            const RudaMirRecordConstruction *construction =
                ruda_mir_record_construction(
                    function,
                    instruction->record_construction
                );
            const RudaMirRecord *record = construction != NULL
                ? ruda_mir_record(function, construction->record)
                : NULL;
            size_t argument_end = 0;
            size_t index = 0;
            if (construction == NULL || record == NULL
                || instruction->record != construction->record
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->record_arguments.length
                || construction->argument_count != record->field_count) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != record->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const RudaMirRecordArgument *argument =
                    ruda_mir_record_argument(
                        function,
                        construction->argument_start + index
                    );
                const RudaMirRecordField *field = ruda_mir_record_field(
                    function,
                    record,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (field == NULL || actual != argument->type
                    || argument->type != field->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_RECORD_GET:
        case RUDA_MIR_RECORD_SET: {
            const RudaMirRecord *record = ruda_mir_record(
                function,
                instruction->record
            );
            const RudaMirRecordField *field = ruda_mir_record_field(
                function,
                record,
                instruction->field
            );
            if (record == NULL || field == NULL) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID
                || (instruction->kind == RUDA_MIR_RECORD_SET
                    && second_type == RUDA_TYPE_INVALID)) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != record->type || target_type != field->type
                || (instruction->kind == RUDA_MIR_RECORD_SET
                    && (second_type != field->type || !field->is_mutable))) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ENUM_NEW: {
            const RudaMirEnumConstruction *construction =
                ruda_mir_enum_construction(
                    function,
                    instruction->enum_construction
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
            size_t index = 0;
            if (construction == NULL || enum_layout == NULL || variant == NULL
                || instruction->enum_id != construction->enum_id
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->enum_arguments.length
                || construction->argument_count != variant->payload_count) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != enum_layout->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const RudaMirEnumArgument *argument = ruda_mir_enum_argument(
                    function,
                    construction->argument_start + index
                );
                const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
                    function,
                    variant,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (payload == NULL || actual != argument->type
                    || argument->type != payload->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ENUM_GET_PAYLOAD: {
            const RudaMirEnum *enum_layout = ruda_mir_enum(
                function,
                instruction->enum_id
            );
            const RudaMirEnumVariant *variant = ruda_mir_enum_variant(
                function,
                enum_layout,
                instruction->variant
            );
            const RudaMirEnumPayload *payload =
                instruction->kind == RUDA_MIR_ENUM_GET_PAYLOAD
                    ? ruda_mir_enum_payload(
                        function,
                        variant,
                        instruction->field
                    )
                    : NULL;
            if (enum_layout == NULL || variant == NULL
                || (instruction->kind == RUDA_MIR_ENUM_GET_PAYLOAD
                    && payload == NULL)) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (first_type == RUDA_TYPE_INVALID) {
                return RUDA_MIR_VERIFY_INVALID_REGISTER;
            }
            if (first_type != enum_layout->type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ENUM_IS_VARIANT) {
                return target_type == RUDA_TYPE_BOOL
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            if (instruction->kind == RUDA_MIR_ENUM_REQUIRE_VARIANT) {
                return target_type == enum_layout->type
                    ? RUDA_MIR_VERIFY_OK
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            return target_type == payload->type
                ? RUDA_MIR_VERIFY_OK
                : RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        case RUDA_MIR_CALL: {
            const RudaMirCall *call = ruda_mir_call(
                function,
                instruction->call
            );
            size_t argument_end = 0;
            size_t index = 0;
            if (call == NULL || call->result_type == RUDA_TYPE_NEVER
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            }
            if (target_type != call->result_type) {
                return RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
            for (index = call->argument_start; index < argument_end;
                index += 1) {
                const RudaMirCallArgument *argument = ruda_mir_call_argument(
                    function,
                    index
                );
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    return RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
                if (actual != argument->type) {
                    return RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            }
            return RUDA_MIR_VERIFY_OK;
        }
    }
    return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
}

static bool ruda_mir_instruction_uses_first(RudaMirInstructionKind kind) {
    return kind != RUDA_MIR_CONSTANT_I64 && kind != RUDA_MIR_CONSTANT_F64
        && kind != RUDA_MIR_CONSTANT_BOOL
        && kind != RUDA_MIR_CONSTANT_STRING && kind != RUDA_MIR_RECORD_NEW
        && kind != RUDA_MIR_ENUM_NEW && kind != RUDA_MIR_CALL;
}

static bool ruda_mir_instruction_uses_second(RudaMirInstructionKind kind) {
    switch (kind) {
        case RUDA_MIR_ADD_CHECKED_I64:
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
        case RUDA_MIR_DIVIDE_CHECKED_I64:
        case RUDA_MIR_REMAINDER_CHECKED_I64:
        case RUDA_MIR_BITWISE_AND_I64:
        case RUDA_MIR_BITWISE_XOR_I64:
        case RUDA_MIR_BITWISE_OR_I64:
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
        case RUDA_MIR_SHIFT_RIGHT_I64:
        case RUDA_MIR_ADD_F64:
        case RUDA_MIR_SUBTRACT_F64:
        case RUDA_MIR_MULTIPLY_F64:
        case RUDA_MIR_DIVIDE_F64:
        case RUDA_MIR_EQUAL_I64:
        case RUDA_MIR_EQUAL_F64:
        case RUDA_MIR_EQUAL_BOOL:
        case RUDA_MIR_EQUAL_STRING:
        case RUDA_MIR_LESS_I64:
        case RUDA_MIR_LESS_EQUAL_I64:
        case RUDA_MIR_LESS_F64:
        case RUDA_MIR_LESS_EQUAL_F64:
        case RUDA_MIR_RECORD_SET:
        case RUDA_MIR_RECORD_IDENTITY_SAME:
            return true;
        case RUDA_MIR_CONSTANT_I64:
        case RUDA_MIR_CONSTANT_F64:
        case RUDA_MIR_CONSTANT_BOOL:
        case RUDA_MIR_CONSTANT_STRING:
        case RUDA_MIR_COPY:
        case RUDA_MIR_NEGATE_CHECKED_I64:
        case RUDA_MIR_NEGATE_F64:
        case RUDA_MIR_NOT_BOOL:
        case RUDA_MIR_BITWISE_NOT_I64:
        case RUDA_MIR_RECORD_NEW:
        case RUDA_MIR_RECORD_GET:
        case RUDA_MIR_ENUM_NEW:
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ENUM_GET_PAYLOAD:
        case RUDA_MIR_CALL:
            return false;
    }
    return false;
}

static bool ruda_mir_uses_f64(const RudaMirFunction *function) {
    size_t index = 0;
    if (function == NULL || function->result_type == RUDA_TYPE_F64) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && *type == RUDA_TYPE_F64) {
            return true;
        }
    }
    return false;
}

static bool ruda_mir_assigned_register(
    const unsigned char *assigned,
    size_t register_count,
    RudaMirRegister register_id
) {
    return register_id != RUDA_MIR_NO_REGISTER
        && register_id < register_count && assigned[register_id] != 0;
}

static void ruda_mir_transfer_assignments(
    const RudaMirBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t index = 0;
    for (index = 0; index < block->instructions.length; index += 1) {
        const RudaMirInstruction *instruction = ruda_vector_at_const(
            &block->instructions,
            index
        );
        if (instruction != NULL && instruction->target < register_count) {
            assigned[instruction->target] = 1;
        }
    }
}

static bool ruda_mir_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (!ruda_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

RudaMirVerifyStatus ruda_mir_verify(const RudaMirFunction *function) {
    size_t block_count = 0;
    size_t register_count = 0;
    size_t cell_count = 0;
    size_t cell_allocation_count = 0;
    size_t scratch_allocation_count = 0;
    size_t offset_count = 0;
    size_t edge_count = 0;
    size_t predecessor_counts_size = 0;
    size_t predecessor_offsets_size = 0;
    size_t predecessors_size = 0;
    size_t block_index = 0;
    size_t instruction_total = 0;
    size_t *predecessor_counts = NULL;
    size_t *predecessor_offsets = NULL;
    RudaMirBlockId *predecessors = NULL;
    unsigned char *incoming = NULL;
    unsigned char *outgoing = NULL;
    unsigned char *scratch = NULL;
    RudaMap record_types;
    size_t iteration = 0;
    size_t work = 0;
    bool changed = false;
    bool record_types_initialized = false;
    RudaMirVerifyStatus status = RUDA_MIR_VERIFY_OK;
    if (function == NULL || !function->initialized
        || function->result_type == RUDA_TYPE_INVALID
        || function->parameter_count > function->register_types.length
        || function->blocks.length == 0
        || function->blocks.length > function->options.max_blocks
        || function->register_types.length > function->options.max_registers
        || function->instruction_count > function->options.max_instructions
        || function->calls.length > function->options.max_calls
        || function->call_arguments.length
            > function->options.max_call_arguments
        || function->records.length > function->options.max_record_layouts
        || function->record_fields.length
            > function->options.max_record_fields
        || function->record_constructions.length
            > function->options.max_record_constructions
        || function->record_arguments.length
            > function->options.max_record_arguments
        || function->enums.length > function->options.max_enum_layouts
        || function->enum_variants.length
            > function->options.max_enum_variants
        || function->enum_payloads.length
            > function->options.max_enum_payloads
        || function->enum_constructions.length
            > function->options.max_enum_constructions
        || function->enum_arguments.length
            > function->options.max_enum_arguments
        || function->string_constants.length
            > function->options.max_string_constants
        || function->string_bytes > function->options.max_string_bytes
        || function->options.max_verify_work == 0) {
        return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
    }
    if (function->records.length != 0 || function->enums.length != 0) {
        if (!ruda_map_init(
                &record_types,
                function->allocator,
                sizeof(RudaMirRecordId),
                UINT64_C(0x725564614d495231),
                UINT64_C(0x725564614d495232)
            )) {
            return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        }
        record_types_initialized = true;
    }
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
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    constant->value.length,
                    function->options.max_verify_work
                ) || !ruda_mir_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
        if (verified_string_bytes != function->string_bytes) {
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
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
        if (record == NULL || record->type == RUDA_TYPE_INVALID
            || !ruda_size_add(
                record->field_start,
                record->field_count,
                &field_end
            ) || field_end > function->record_fields.length) {
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (ruda_map_get_const(
                &record_types,
                &record->type,
                sizeof(record->type)
            ) != NULL) {
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        {
            RudaMirRecordId record_id = (RudaMirRecordId)block_index;
            if (!ruda_map_put(
                    &record_types,
                    &record->type,
                    sizeof(record->type),
                    &record_id
                )) {
                status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
        if (!ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        for (field_index = record->field_start; field_index < field_end;
            field_index += 1) {
            const RudaMirRecordField *field = ruda_vector_at_const(
                &function->record_fields,
                field_index
            );
            if (field == NULL || field->type == RUDA_TYPE_INVALID) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
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
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (!ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
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
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
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
        if (enum_layout == NULL || enum_layout->type == RUDA_TYPE_INVALID
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
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (ruda_map_get_const(
                &record_types,
                &enum_layout->type,
                sizeof(enum_layout->type)
            ) != NULL) {
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        {
            RudaMirEnumId enum_id = (RudaMirEnumId)block_index;
            if (!ruda_map_put(
                    &record_types,
                    &enum_layout->type,
                    sizeof(enum_layout->type),
                    &enum_id
                )) {
                status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
        if (!ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
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
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            for (payload_index = 0; payload_index < variant->payload_count;
                payload_index += 1) {
                const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
                    function,
                    variant,
                    payload_index
                );
                if (payload == NULL || payload->type == RUDA_TYPE_INVALID) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        1,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
            payload_cursor = local_payload_end;
        }
        if (payload_cursor != enum_layout->payload_count) {
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
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
            status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (!ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
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
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
    }
    block_count = function->blocks.length;
    register_count = function->register_types.length;
    if (!ruda_size_multiply(block_count, register_count, &cell_count)
        || cell_count > RUDA_MIR_MAX_DATAFLOW_CELLS
        || !ruda_size_add(block_count, 1, &offset_count)
        || !ruda_size_multiply(
            block_count,
            sizeof(size_t),
            &predecessor_counts_size
        ) || !ruda_size_multiply(
            offset_count,
            sizeof(size_t),
            &predecessor_offsets_size
        )) {
        return RUDA_MIR_VERIFY_LIMIT_REACHED;
    }
    cell_allocation_count = cell_count == 0 ? 1 : cell_count;
    scratch_allocation_count = register_count == 0 ? 1 : register_count;
    predecessor_counts = ruda_allocate_array(
        &function->allocator,
        block_count,
        sizeof(size_t)
    );
    incoming = ruda_allocate_array(
        &function->allocator,
        cell_allocation_count,
        sizeof(unsigned char)
    );
    outgoing = ruda_allocate_array(
        &function->allocator,
        cell_allocation_count,
        sizeof(unsigned char)
    );
    scratch = ruda_allocate_array(
        &function->allocator,
        scratch_allocation_count,
        sizeof(unsigned char)
    );
    if (predecessor_counts == NULL || incoming == NULL || outgoing == NULL
        || scratch == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(predecessor_counts, 0, predecessor_counts_size);
    memset(incoming, 0, cell_allocation_count);
    memset(outgoing, 0, cell_allocation_count);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (!ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        if (block == NULL || !block->initialized
            || block->terminator.kind == RUDA_MIR_TERMINATOR_UNSET) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
        if (!ruda_size_add(
                instruction_total,
                block->instructions.length,
                &instruction_total
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            size_t instruction_work = 1;
            if (instruction != NULL
                && instruction->kind == RUDA_MIR_CALL) {
                const RudaMirCall *call = ruda_mir_call(
                    function,
                    instruction->call
                );
                size_t argument_end = 0;
                if (call == NULL || !ruda_size_add(
                        call->argument_start,
                        call->argument_count,
                        &argument_end
                    ) || argument_end > function->call_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_size_add(
                        instruction_work,
                        call->argument_count,
                        &instruction_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (instruction != NULL
                && instruction->kind == RUDA_MIR_RECORD_NEW) {
                const RudaMirRecordConstruction *construction =
                    ruda_mir_record_construction(
                        function,
                        instruction->record_construction
                    );
                if (construction == NULL || !ruda_size_add(
                        instruction_work,
                        construction->argument_count,
                        &instruction_work
                    )) {
                    status = construction == NULL
                        ? RUDA_MIR_VERIFY_INVALID_ARGUMENT
                        : RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (instruction != NULL
                && instruction->kind == RUDA_MIR_ENUM_NEW) {
                const RudaMirEnumConstruction *construction =
                    ruda_mir_enum_construction(
                        function,
                        instruction->enum_construction
                    );
                if (construction == NULL || !ruda_size_add(
                        instruction_work,
                        construction->argument_count,
                        &instruction_work
                    )) {
                    status = construction == NULL
                        ? RUDA_MIR_VERIFY_INVALID_ARGUMENT
                        : RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    instruction_work,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            status = instruction != NULL
                ? ruda_mir_validate_instruction_types(function, instruction)
                : RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            if (status != RUDA_MIR_VERIFY_OK) {
                goto cleanup;
            }
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (block->terminator.target >= block_count) {
                status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            if (!ruda_size_add(
                    predecessor_counts[block->terminator.target],
                    1,
                    &predecessor_counts[block->terminator.target]
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (block->terminator.target >= block_count
                || block->terminator.otherwise >= block_count) {
                status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            if (!ruda_mir_register_is(
                    function,
                    block->terminator.value,
                    RUDA_TYPE_BOOL
                )) {
                status = block->terminator.value >= register_count
                    ? RUDA_MIR_VERIFY_INVALID_REGISTER
                    : RUDA_MIR_VERIFY_TYPE_MISMATCH;
                goto cleanup;
            }
            if (!ruda_size_add(
                    predecessor_counts[block->terminator.target],
                    1,
                    &predecessor_counts[block->terminator.target]
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (block->terminator.otherwise != block->terminator.target) {
                if (!ruda_size_add(
                        predecessor_counts[block->terminator.otherwise],
                        1,
                        &predecessor_counts[block->terminator.otherwise]
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
        } else if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_mir_call(
                function,
                block->terminator.call
            );
            size_t argument_end = 0;
            size_t argument_index = 0;
            if (call == NULL || call->result_type != RUDA_TYPE_NEVER
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (!ruda_mir_consume_verify_work(
                    &work,
                    call->argument_count,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            for (argument_index = call->argument_start;
                argument_index < argument_end; argument_index += 1) {
                const RudaMirCallArgument *argument =
                    ruda_mir_call_argument(function, argument_index);
                RudaTypeId actual = argument != NULL
                    ? ruda_mir_register_type(
                        function,
                        argument->register_id
                    )
                    : RUDA_TYPE_INVALID;
                if (actual == RUDA_TYPE_INVALID) {
                    status = RUDA_MIR_VERIFY_INVALID_REGISTER;
                    goto cleanup;
                }
                if (actual != argument->type) {
                    status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
                    goto cleanup;
                }
            }
        } else if (block->terminator.kind != RUDA_MIR_RETURN
            && block->terminator.kind != RUDA_MIR_PANIC) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
    }
    if (instruction_total != function->instruction_count) {
        status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        goto cleanup;
    }
    predecessor_offsets = ruda_allocate_array(
        &function->allocator,
        offset_count,
        sizeof(size_t)
    );
    if (predecessor_offsets == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    predecessor_offsets[0] = 0;
    for (block_index = 0; block_index < block_count; block_index += 1) {
        if (!ruda_size_add(
                predecessor_offsets[block_index],
                predecessor_counts[block_index],
                &predecessor_offsets[block_index + 1]
            ) || !ruda_mir_consume_verify_work(
                &work,
                1,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
    }
    edge_count = predecessor_offsets[block_count];
    if (!ruda_size_multiply(
            edge_count == 0 ? 1 : edge_count,
            sizeof(RudaMirBlockId),
            &predecessors_size
        )) {
        status = RUDA_MIR_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    predecessors = ruda_allocate_array(
        &function->allocator,
        edge_count == 0 ? 1 : edge_count,
        sizeof(RudaMirBlockId)
    );
    if (predecessors == NULL) {
        status = RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(predecessor_counts, 0, predecessor_counts_size);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        RudaMirBlockId targets[2];
        size_t target_count = 0;
        size_t target_index = 0;
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            targets[target_count] = block->terminator.target;
            target_count += 1;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            targets[target_count] = block->terminator.target;
            target_count += 1;
            if (block->terminator.otherwise != block->terminator.target) {
                targets[target_count] = block->terminator.otherwise;
                target_count += 1;
            }
        }
        for (target_index = 0; target_index < target_count;
            target_index += 1) {
            size_t target = targets[target_index];
            size_t position = predecessor_offsets[target]
                + predecessor_counts[target];
            if (position >= edge_count) {
                status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            predecessors[position] = (RudaMirBlockId)block_index;
            predecessor_counts[target] += 1;
            if (!ruda_mir_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
    }
    if (register_count != 0) {
        size_t register_index = 0;
        for (register_index = 0;
            register_index < function->parameter_count;
            register_index += 1) {
            incoming[register_index] = 1;
        }
        for (block_index = 1; block_index < block_count; block_index += 1) {
            if (predecessor_offsets[block_index]
                != predecessor_offsets[block_index + 1]) {
                memset(
                    incoming + block_index * register_count,
                    1,
                    register_count
                );
                memset(
                    outgoing + block_index * register_count,
                    1,
                    register_count
                );
            }
        }
    }
    do {
        changed = false;
        for (block_index = 0; block_index < block_count; block_index += 1) {
            const RudaMirBlock *block = ruda_vector_at_const(
                &function->blocks,
                block_index
            );
            unsigned char *block_in = incoming + block_index * register_count;
            unsigned char *block_out = outgoing + block_index * register_count;
            size_t register_index = 0;
            size_t predecessor_start = predecessor_offsets[block_index];
            size_t predecessor_end = predecessor_offsets[block_index + 1];
            size_t predecessor_count = predecessor_end - predecessor_start;
            size_t predecessor_work = 0;
            size_t transfer_work = 0;
            if (!ruda_size_multiply(
                    register_count,
                    predecessor_count,
                    &predecessor_work
                ) || !ruda_size_add(
                    block->instructions.length,
                    register_count,
                    &transfer_work
                ) || !ruda_size_add(
                    transfer_work,
                    register_count,
                    &transfer_work
                ) || !ruda_mir_consume_verify_work(
                    &work,
                    predecessor_work,
                    function->options.max_verify_work
                ) || !ruda_mir_consume_verify_work(
                    &work,
                    transfer_work,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
            if (block_index != 0 && predecessor_count != 0) {
                for (register_index = 0;
                    register_index < register_count;
                    register_index += 1) {
                    size_t predecessor_index = predecessor_start;
                    RudaMirBlockId predecessor = predecessors[
                        predecessor_index
                    ];
                    unsigned char intersection = outgoing[
                        (size_t)predecessor * register_count + register_index
                    ];
                    for (predecessor_index += 1;
                        predecessor_index < predecessor_end;
                        predecessor_index += 1) {
                        predecessor = predecessors[predecessor_index];
                        intersection = (unsigned char)(intersection
                            && outgoing[
                                (size_t)predecessor * register_count
                                    + register_index
                            ]);
                    }
                    if (block_in[register_index] != intersection) {
                        block_in[register_index] = intersection;
                        changed = true;
                    }
                }
            }
            memcpy(scratch, block_in, register_count);
            ruda_mir_transfer_assignments(block, scratch, register_count);
            if (memcmp(block_out, scratch, register_count) != 0) {
                memcpy(block_out, scratch, register_count);
                changed = true;
            }
        }
        iteration += 1;
        if (iteration > block_count + 1) {
            status = RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
    } while (changed);

    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        unsigned char *assigned = incoming + block_index * register_count;
        unsigned char *working = scratch;
        size_t instruction_index = 0;
        size_t validation_work = 0;
        if (!ruda_size_add(
                register_count,
                block->instructions.length,
                &validation_work
            ) || !ruda_size_add(
                validation_work,
                1,
                &validation_work
            ) || !ruda_mir_consume_verify_work(
                &work,
                validation_work,
                function->options.max_verify_work
            )) {
            status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        memcpy(working, assigned, register_count);
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (ruda_mir_instruction_uses_first(instruction->kind)
                && !ruda_mir_assigned_register(
                    working,
                    register_count,
                    instruction->first
                )) {
                status = RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER;
                goto cleanup;
            }
            if (ruda_mir_instruction_uses_second(instruction->kind)
                && !ruda_mir_assigned_register(
                    working,
                    register_count,
                    instruction->second
                )) {
                status = RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER;
                goto cleanup;
            }
            if (instruction->kind == RUDA_MIR_CALL) {
                const RudaMirCall *call = ruda_mir_call(
                    function,
                    instruction->call
                );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (call == NULL || !ruda_size_add(
                        call->argument_start,
                        call->argument_count,
                        &argument_end
                    ) || argument_end > function->call_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        call->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = call->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirCallArgument *argument =
                        ruda_mir_call_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            } else if (instruction->kind == RUDA_MIR_RECORD_NEW) {
                const RudaMirRecordConstruction *construction =
                    ruda_mir_record_construction(
                        function,
                        instruction->record_construction
                    );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (construction == NULL || !ruda_size_add(
                        construction->argument_start,
                        construction->argument_count,
                        &argument_end
                    ) || argument_end > function->record_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        construction->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = construction->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirRecordArgument *argument =
                        ruda_mir_record_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            } else if (instruction->kind == RUDA_MIR_ENUM_NEW) {
                const RudaMirEnumConstruction *construction =
                    ruda_mir_enum_construction(
                        function,
                        instruction->enum_construction
                    );
                size_t argument_end = 0;
                size_t argument_index = 0;
                if (construction == NULL || !ruda_size_add(
                        construction->argument_start,
                        construction->argument_count,
                        &argument_end
                    ) || argument_end > function->enum_arguments.length) {
                    status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
                    goto cleanup;
                }
                if (!ruda_mir_consume_verify_work(
                        &work,
                        construction->argument_count,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_MIR_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
                for (argument_index = construction->argument_start;
                    argument_index < argument_end; argument_index += 1) {
                    const RudaMirEnumArgument *argument =
                        ruda_mir_enum_argument(function, argument_index);
                    if (argument == NULL || !ruda_mir_assigned_register(
                            working,
                            register_count,
                            argument->register_id
                        )) {
                        status = argument != NULL
                                && argument->register_id < register_count
                            ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                            : RUDA_MIR_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                }
            }
            working[instruction->target] = 1;
        }
        if (block->terminator.kind == RUDA_MIR_RETURN) {
            if (function->result_type == RUDA_TYPE_NEVER) {
                status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
            } else if (function->result_type == RUDA_TYPE_VOID) {
                if (block->terminator.value != RUDA_MIR_NO_REGISTER) {
                    status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
                }
            } else if (!ruda_mir_assigned_register(
                    working,
                    register_count,
                    block->terminator.value
                )) {
                status = block->terminator.value < register_count
                    ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                    : RUDA_MIR_VERIFY_INVALID_REGISTER;
            } else if (ruda_mir_register_type(
                    function,
                    block->terminator.value
                ) != function->result_type) {
                status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
        } else if (block->terminator.kind == RUDA_MIR_PANIC) {
            if (!ruda_mir_assigned_register(
                    working,
                    register_count,
                    block->terminator.value
                )) {
                status = block->terminator.value < register_count
                    ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                    : RUDA_MIR_VERIFY_INVALID_REGISTER;
            } else if (ruda_mir_register_type(
                    function,
                    block->terminator.value
                ) != RUDA_TYPE_STRING) {
                status = RUDA_MIR_VERIFY_TYPE_MISMATCH;
            }
        } else if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_mir_call(
                function,
                block->terminator.call
            );
            size_t argument_end = 0;
            size_t argument_index = 0;
            if (call == NULL || call->result_type != RUDA_TYPE_NEVER
                || !ruda_size_add(
                    call->argument_start,
                    call->argument_count,
                    &argument_end
                ) || argument_end > function->call_arguments.length) {
                status = RUDA_MIR_VERIFY_INVALID_ARGUMENT;
            } else if (!ruda_mir_consume_verify_work(
                    &work,
                    call->argument_count,
                    function->options.max_verify_work
                )) {
                status = RUDA_MIR_VERIFY_LIMIT_REACHED;
            }
            for (argument_index = call != NULL ? call->argument_start : 0;
                status == RUDA_MIR_VERIFY_OK && argument_index < argument_end;
                argument_index += 1) {
                const RudaMirCallArgument *argument =
                    ruda_mir_call_argument(function, argument_index);
                if (argument == NULL || !ruda_mir_assigned_register(
                        working,
                        register_count,
                        argument->register_id
                    )) {
                    status = argument != NULL
                            && argument->register_id < register_count
                        ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                        : RUDA_MIR_VERIFY_INVALID_REGISTER;
                }
            }
        } else if (block->terminator.kind == RUDA_MIR_BRANCH
            && !ruda_mir_assigned_register(
                working,
                register_count,
                block->terminator.value
            )) {
            status = block->terminator.value < register_count
                ? RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER
                : RUDA_MIR_VERIFY_INVALID_REGISTER;
        }
        if (status != RUDA_MIR_VERIFY_OK) {
            goto cleanup;
        }
    }

cleanup:
    if (record_types_initialized) {
        ruda_map_dispose(&record_types);
    }
    if (scratch != NULL) {
        ruda_deallocate(
            &function->allocator,
            scratch,
            scratch_allocation_count
        );
    }
    if (predecessors != NULL) {
        ruda_deallocate(
            &function->allocator,
            predecessors,
            predecessors_size
        );
    }
    if (predecessor_offsets != NULL) {
        ruda_deallocate(
            &function->allocator,
            predecessor_offsets,
            predecessor_offsets_size
        );
    }
    if (outgoing != NULL) {
        ruda_deallocate(
            &function->allocator,
            outgoing,
            cell_allocation_count
        );
    }
    if (incoming != NULL) {
        ruda_deallocate(
            &function->allocator,
            incoming,
            cell_allocation_count
        );
    }
    if (predecessor_counts != NULL) {
        ruda_deallocate(
            &function->allocator,
            predecessor_counts,
            predecessor_counts_size
        );
    }
    return status;
}

static RudaMirManagedRecord *ruda_mir_execution_record(
    RudaMirExecution *execution,
    RudaMirReference reference
) {
    RudaMirManagedRecord **record = execution != NULL
            && execution->initialized && reference != RUDA_MIR_INVALID_REFERENCE
        ? ruda_vector_at(&execution->objects, reference)
        : NULL;
    return record != NULL ? *record : NULL;
}

static bool ruda_mir_managed_record_matches_layout(
    const RudaMirFunction *function,
    const RudaMirRecord *layout,
    const RudaMirManagedRecord *record
) {
    const RudaMirRecordField *stored_layout =
        ruda_mir_managed_record_layout_const(record);
    size_t field_index = 0;
    if (function == NULL || layout == NULL || record == NULL
        || record->type != layout->type
        || record->field_count != layout->field_count) {
        return false;
    }
    for (field_index = 0; field_index < layout->field_count;
        field_index += 1) {
        const RudaMirRecordField *expected = ruda_mir_record_field(
            function,
            layout,
            field_index
        );
        if (expected == NULL || stored_layout == NULL
            || stored_layout[field_index].type != expected->type
            || stored_layout[field_index].is_mutable != expected->is_mutable
            || record->fields[field_index].type != expected->type) {
            return false;
        }
    }
    return true;
}

bool ruda_mir_execution_owns_reference(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    RudaMirManagedRecord *const *record = execution != NULL
            && execution->initialized
            && value.as.reference != RUDA_MIR_INVALID_REFERENCE
        ? ruda_vector_at_const(&execution->objects, value.as.reference)
        : NULL;
    return ruda_mir_execution_string_value(execution, value) != NULL
        || ruda_mir_execution_enum_value(execution, value) != NULL
        || (record != NULL && *record != NULL && (*record)->type == value.type);
}

static RudaMirEnumVariant *ruda_mir_managed_enum_variants(
    RudaMirManagedEnum *value
) {
    return value != NULL ? (RudaMirEnumVariant *)(
        value->payload + value->selected_payload_count
    ) : NULL;
}

static RudaMirEnumPayload *ruda_mir_managed_enum_payloads(
    RudaMirManagedEnum *value
) {
    RudaMirEnumVariant *variants = ruda_mir_managed_enum_variants(value);
    return variants != NULL ? (RudaMirEnumPayload *)(
        variants + value->variant_count
    ) : NULL;
}

static bool ruda_mir_managed_enum_matches_layout(
    const RudaMirFunction *function,
    const RudaMirEnum *layout,
    const RudaMirManagedEnum *value
) {
    const RudaMirEnumVariant *stored_variants = NULL;
    const RudaMirEnumPayload *stored_payloads = NULL;
    size_t variant_index = 0;
    size_t payload_index = 0;
    if (function == NULL || layout == NULL || value == NULL
        || value->type != layout->type
        || value->variant_count != layout->variant_count
        || value->layout_payload_count != layout->payload_count
        || value->variant >= value->variant_count) {
        return false;
    }
    stored_variants = (const RudaMirEnumVariant *)(
        value->payload + value->selected_payload_count
    );
    stored_payloads = (const RudaMirEnumPayload *)(
        stored_variants + value->variant_count
    );
    for (variant_index = 0; variant_index < layout->variant_count;
        variant_index += 1) {
        const RudaMirEnumVariant *expected = ruda_mir_enum_variant(
            function,
            layout,
            variant_index
        );
        if (expected == NULL || expected->payload_start < layout->payload_start
            || stored_variants[variant_index].kind != expected->kind
            || stored_variants[variant_index].payload_start
                != expected->payload_start - layout->payload_start
            || stored_variants[variant_index].payload_count
                != expected->payload_count) {
            return false;
        }
    }
    for (payload_index = 0; payload_index < layout->payload_count;
        payload_index += 1) {
        const RudaMirEnumPayload *expected = ruda_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + payload_index
        );
        if (expected == NULL
            || stored_payloads[payload_index].type != expected->type) {
            return false;
        }
    }
    {
        const RudaMirEnumVariant *selected = &stored_variants[value->variant];
        if (value->selected_payload_count != selected->payload_count) {
            return false;
        }
        for (payload_index = 0;
            payload_index < value->selected_payload_count;
            payload_index += 1) {
            size_t layout_index = selected->payload_start + payload_index;
            if (layout_index >= value->layout_payload_count
                || value->payload[payload_index].type
                    != stored_payloads[layout_index].type) {
                return false;
            }
        }
    }
    return true;
}

static RudaMirInterpretStatus ruda_mir_execution_new_record(
    RudaMirExecution *execution,
    RudaTypeId type,
    const RudaMirValue *fields,
    const RudaMirRecordField *layout,
    size_t field_count,
    RudaMirReference *result
) {
    RudaMirManagedRecord *record = NULL;
    size_t field_bytes = 0;
    size_t layout_bytes = 0;
    size_t payload_bytes = 0;
    size_t allocation_size = 0;
    size_t new_total_fields = 0;
    size_t object_count = 0;
    size_t field_index = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || type == RUDA_TYPE_INVALID) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (field_count != 0 && (fields == NULL || layout == NULL)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!ruda_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || object_count >= execution->options.max_objects
        || execution->objects.length >= UINT32_MAX
        || field_count > execution->options.max_fields_per_object
        || !ruda_size_add(
            execution->total_fields,
            field_count,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (!ruda_size_multiply(field_count, sizeof(RudaMirValue), &field_bytes)
        || !ruda_size_multiply(
            field_count,
            sizeof(RudaMirRecordField),
            &layout_bytes
        ) || !ruda_size_add(field_bytes, layout_bytes, &payload_bytes)
        || !ruda_size_add(
            offsetof(RudaMirManagedRecord, fields),
            payload_bytes,
            &allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    for (field_index = 0; field_index < field_count; field_index += 1) {
        if (layout[field_index].type == RUDA_TYPE_INVALID
            || fields[field_index].type != layout[field_index].type) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
    }
    if (!ruda_vector_reserve(
            &execution->objects,
            execution->objects.length + 1
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    record = ruda_allocate(&execution->allocator, allocation_size);
    if (record == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    record->type = type;
    record->field_count = field_count;
    record->allocation_size = allocation_size;
    if (field_count != 0 && fields != NULL && layout != NULL) {
        memcpy(record->fields, fields, field_bytes);
        memcpy(
            ruda_mir_managed_record_layout(record),
            layout,
            layout_bytes
        );
    }
    if (!ruda_vector_push(&execution->objects, &record)) {
        ruda_deallocate(&execution->allocator, record, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    *result = (RudaMirReference)(execution->objects.length - 1);
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_mir_execution_new_string(
    RudaMirExecution *execution,
    const unsigned char *bytes,
    size_t length,
    RudaMirReference *result
) {
    RudaMirManagedString *value = NULL;
    size_t allocation_size = 0;
    size_t new_total = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || execution->strings.length >= execution->options.max_strings
        || execution->strings.length >= UINT32_MAX
        || !ruda_size_add(sizeof(*value), length, &allocation_size)
        || !ruda_size_add(
            execution->total_string_bytes,
            length,
            &new_total
        ) || new_total > execution->options.max_total_string_bytes) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    value = ruda_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->allocation_size = allocation_size;
    value->length = length;
    if (length != 0) {
        memcpy(value->bytes, bytes, length);
    }
    if (!ruda_vector_push(&execution->strings, &value)) {
        ruda_deallocate(&execution->allocator, value, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_string_bytes = new_total;
    *result = (RudaMirReference)(execution->strings.length - 1);
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_mir_execution_new_enum(
    RudaMirExecution *execution,
    const RudaMirFunction *function,
    const RudaMirEnum *enum_layout,
    size_t selected_variant,
    const RudaMirValue *payload_values,
    size_t payload_value_count,
    RudaMirReference *result
) {
    const RudaMirEnumVariant *selected = ruda_mir_enum_variant(
        function,
        enum_layout,
        selected_variant
    );
    RudaMirManagedEnum *value = NULL;
    size_t value_bytes = 0;
    size_t variant_bytes = 0;
    size_t payload_layout_bytes = 0;
    size_t allocation_size = 0;
    size_t object_slots = 0;
    size_t object_count = 0;
    size_t new_total_fields = 0;
    size_t index = 0;
    if (execution == NULL || !execution->initialized || function == NULL
        || enum_layout == NULL || selected == NULL || result == NULL
        || selected->payload_count != payload_value_count
        || (payload_values == NULL && payload_value_count != 0)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!ruda_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !ruda_size_add(
            payload_value_count,
            enum_layout->variant_count,
            &object_slots
        ) || !ruda_size_add(
            object_slots,
            enum_layout->payload_count,
            &object_slots
        ) || object_count >= execution->options.max_objects
        || execution->enums.length >= UINT32_MAX
        || object_slots > execution->options.max_fields_per_object
        || !ruda_size_add(
            execution->total_fields,
            object_slots,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields
        || !ruda_size_multiply(
            payload_value_count,
            sizeof(RudaMirValue),
            &value_bytes
        ) || !ruda_size_multiply(
            enum_layout->variant_count,
            sizeof(RudaMirEnumVariant),
            &variant_bytes
        ) || !ruda_size_multiply(
            enum_layout->payload_count,
            sizeof(RudaMirEnumPayload),
            &payload_layout_bytes
        ) || !ruda_size_add(
            offsetof(RudaMirManagedEnum, payload),
            value_bytes,
            &allocation_size
        ) || !ruda_size_add(
            allocation_size,
            variant_bytes,
            &allocation_size
        ) || !ruda_size_add(
            allocation_size,
            payload_layout_bytes,
            &allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    for (index = 0; index < payload_value_count; index += 1) {
        const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
            function,
            selected,
            index
        );
        if (payload == NULL || payload_values[index].type != payload->type) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
    }
    if (!ruda_vector_reserve(
            &execution->enums,
            execution->enums.length + 1
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = ruda_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->type = enum_layout->type;
    value->variant = selected_variant;
    value->variant_count = enum_layout->variant_count;
    value->selected_payload_count = payload_value_count;
    value->layout_payload_count = enum_layout->payload_count;
    value->allocation_size = allocation_size;
    if (payload_value_count != 0 && payload_values != NULL) {
        memcpy(value->payload, payload_values, value_bytes);
    }
    for (index = 0; index < enum_layout->variant_count; index += 1) {
        const RudaMirEnumVariant *variant = ruda_mir_enum_variant(
            function,
            enum_layout,
            index
        );
        RudaMirEnumVariant *stored = ruda_mir_managed_enum_variants(value);
        if (variant == NULL || stored == NULL
            || variant->payload_start < enum_layout->payload_start) {
            ruda_deallocate(&execution->allocator, value, allocation_size);
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        stored[index] = *variant;
        stored[index].payload_start -= enum_layout->payload_start;
    }
    if (enum_layout->payload_count != 0) {
        const RudaMirEnumPayload *layout_payloads = ruda_vector_at_const(
            &function->enum_payloads,
            enum_layout->payload_start
        );
        RudaMirEnumPayload *stored_payloads =
            ruda_mir_managed_enum_payloads(value);
        if (layout_payloads == NULL || stored_payloads == NULL) {
            ruda_deallocate(&execution->allocator, value, allocation_size);
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        memcpy(
            stored_payloads,
            layout_payloads,
            payload_layout_bytes
        );
    }
    if (!ruda_vector_push(&execution->enums, &value)) {
        ruda_deallocate(&execution->allocator, value, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    *result = (RudaMirReference)(execution->enums.length - 1);
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_mir_invoke_call(
    const RudaMirFunction *function,
    RudaMirCallId call_id,
    const RudaMirValue *registers,
    size_t register_count,
    const RudaAllocator *allocator,
    RudaMirInterpretOptions options,
    RudaMirValue *result
) {
    const RudaMirCall *call = ruda_mir_call(function, call_id);
    RudaMirValue *arguments = NULL;
    size_t argument_bytes = 0;
    size_t argument_end = 0;
    size_t index = 0;
    RudaMirInterpretStatus status;
    if (function == NULL || registers == NULL || allocator == NULL
        || result == NULL || call == NULL || options.call == NULL
        || !ruda_size_add(
            call->argument_start,
            call->argument_count,
            &argument_end
        ) || argument_end > function->call_arguments.length
        || !ruda_size_multiply(
            call->argument_count,
            sizeof(RudaMirValue),
            &argument_bytes
        )) {
        return options.call == NULL
            ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
            : RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    if (argument_bytes != 0) {
        arguments = ruda_allocate(allocator, argument_bytes);
        if (arguments == NULL) {
            return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        }
    }
    if (call->argument_count != 0 && arguments == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    for (index = 0; index < call->argument_count; index += 1) {
        const RudaMirCallArgument *argument = ruda_mir_call_argument(
            function,
            call->argument_start + index
        );
        if (argument == NULL || argument->register_id >= register_count) {
            ruda_deallocate(allocator, arguments, argument_bytes);
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        arguments[index] = registers[argument->register_id];
    }
    status = options.call(
        options.call_context,
        call->callee,
        arguments,
        call->argument_count,
        result
    );
    ruda_deallocate(allocator, arguments, argument_bytes);
    return status;
}

static RudaMirInterpretStatus ruda_mir_execute_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    RudaMirValue *registers,
    size_t register_count,
    const RudaAllocator *allocator,
    RudaMirInterpretOptions options
) {
    RudaMirValue *target = NULL;
    const RudaMirValue *first = NULL;
    const RudaMirValue *second = NULL;
    if (function == NULL || instruction == NULL || registers == NULL
        || allocator == NULL
        || instruction->target >= register_count) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    target = &registers[instruction->target];
    first = target;
    second = target;
    if (ruda_mir_instruction_uses_first(instruction->kind)) {
        if (instruction->first >= register_count) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        first = &registers[instruction->first];
    }
    if (ruda_mir_instruction_uses_second(instruction->kind)) {
        if (instruction->second >= register_count) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        second = &registers[instruction->second];
    }
    switch (instruction->kind) {
        case RUDA_MIR_CONSTANT_I64:
            target->as.i64 = instruction->integer;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_CONSTANT_F64:
            target->as.f64_bits = instruction->f64_bits;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_CONSTANT_BOOL:
            target->as.boolean = instruction->boolean;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_CONSTANT_STRING: {
            const RudaMirStringConstant *constant = ruda_mir_string_constant(
                function,
                instruction->string_constant
            );
            if (constant == NULL || options.execution == NULL) {
                return options.execution == NULL
                    ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                    : RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            return ruda_mir_execution_new_string(
                options.execution,
                constant->value.data,
                constant->value.length,
                &target->as.reference
            );
        }
        case RUDA_MIR_COPY:
            *target = *first;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_ADD_CHECKED_I64:
            return ruda_checked_i64_add(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            ) == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
            return ruda_checked_i64_subtract(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            ) == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
            return ruda_checked_i64_multiply(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            ) == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        case RUDA_MIR_DIVIDE_CHECKED_I64: {
            RudaCheckedI64Status checked = ruda_checked_i64_divide(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == RUDA_CHECKED_I64_DIVISION_BY_ZERO) {
                return RUDA_MIR_INTERPRET_DIVISION_BY_ZERO;
            }
            return checked == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        }
        case RUDA_MIR_REMAINDER_CHECKED_I64: {
            RudaCheckedI64Status checked = ruda_checked_i64_remainder(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == RUDA_CHECKED_I64_DIVISION_BY_ZERO) {
                return RUDA_MIR_INTERPRET_DIVISION_BY_ZERO;
            }
            return checked == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        }
        case RUDA_MIR_NEGATE_CHECKED_I64:
            return ruda_checked_i64_negate(
                first->as.i64,
                &target->as.i64
            ) == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        case RUDA_MIR_ADD_F64:
            target->as.f64_bits = ruda_f64_add(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_SUBTRACT_F64:
            target->as.f64_bits = ruda_f64_subtract(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_MULTIPLY_F64:
            target->as.f64_bits = ruda_f64_multiply(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_DIVIDE_F64:
            target->as.f64_bits = ruda_f64_divide(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_NEGATE_F64:
            target->as.f64_bits = ruda_f64_negate(first->as.f64_bits);
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_NOT_BOOL:
            target->as.boolean = !first->as.boolean;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_BITWISE_NOT_I64:
            target->as.i64 = ruda_i64_bitwise_not(first->as.i64);
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_BITWISE_AND_I64:
            target->as.i64 = ruda_i64_bitwise_and(
                first->as.i64,
                second->as.i64
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_BITWISE_XOR_I64:
            target->as.i64 = ruda_i64_bitwise_xor(
                first->as.i64,
                second->as.i64
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_BITWISE_OR_I64:
            target->as.i64 = ruda_i64_bitwise_or(
                first->as.i64,
                second->as.i64
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64: {
            RudaCheckedI64Status checked = ruda_checked_i64_shift_left(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == RUDA_CHECKED_I64_INVALID_SHIFT) {
                return RUDA_MIR_INTERPRET_INVALID_SHIFT;
            }
            return checked == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        }
        case RUDA_MIR_SHIFT_RIGHT_I64: {
            RudaCheckedI64Status checked = ruda_checked_i64_shift_right(
                first->as.i64,
                second->as.i64,
                &target->as.i64
            );
            if (checked == RUDA_CHECKED_I64_INVALID_SHIFT) {
                return RUDA_MIR_INTERPRET_INVALID_SHIFT;
            }
            return checked == RUDA_CHECKED_I64_OK
                ? RUDA_MIR_INTERPRET_OK
                : RUDA_MIR_INTERPRET_OVERFLOW;
        }
        case RUDA_MIR_EQUAL_I64:
            target->as.boolean = first->as.i64 == second->as.i64;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_EQUAL_F64:
            target->as.boolean = ruda_f64_equal(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_EQUAL_BOOL:
            target->as.boolean = first->as.boolean == second->as.boolean;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_EQUAL_STRING: {
            const RudaMirManagedString *left =
                ruda_mir_execution_string_value(options.execution, *first);
            const RudaMirManagedString *right =
                ruda_mir_execution_string_value(options.execution, *second);
            if (left == NULL || right == NULL) {
                return options.execution == NULL
                    ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                    : RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
            target->as.boolean = left->length == right->length
                && (left->length == 0
                    || memcmp(left->bytes, right->bytes, left->length) == 0);
            return RUDA_MIR_INTERPRET_OK;
        }
        case RUDA_MIR_RECORD_IDENTITY_SAME: {
            const RudaMirRecord *layout = ruda_mir_record(
                function,
                instruction->record
            );
            RudaMirManagedRecord *left = options.execution != NULL
                ? ruda_mir_execution_record(
                    options.execution,
                    first->as.reference
                )
                : NULL;
            RudaMirManagedRecord *right = options.execution != NULL
                ? ruda_mir_execution_record(
                    options.execution,
                    second->as.reference
                )
                : NULL;
            if (options.execution == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (layout == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            if (left == NULL || right == NULL
                || left->type != layout->type
                || right->type != layout->type) {
                return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (!ruda_mir_managed_record_matches_layout(
                    function,
                    layout,
                    left
                ) || !ruda_mir_managed_record_matches_layout(
                    function,
                    layout,
                    right
                )) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            target->as.boolean = first->as.reference
                == second->as.reference;
            return RUDA_MIR_INTERPRET_OK;
        }
        case RUDA_MIR_LESS_I64:
            target->as.boolean = first->as.i64 < second->as.i64;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_LESS_F64:
            target->as.boolean = ruda_f64_less(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_LESS_EQUAL_I64:
            target->as.boolean = first->as.i64 <= second->as.i64;
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_LESS_EQUAL_F64:
            target->as.boolean = ruda_f64_less_equal(
                first->as.f64_bits,
                second->as.f64_bits
            );
            return RUDA_MIR_INTERPRET_OK;
        case RUDA_MIR_RECORD_NEW: {
            const RudaMirRecordConstruction *construction =
                ruda_mir_record_construction(
                    function,
                    instruction->record_construction
                );
            const RudaMirRecord *record = construction != NULL
                ? ruda_mir_record(function, construction->record)
                : NULL;
            const RudaMirRecordField *layout_fields = record != NULL
                    && record->field_count != 0
                ? ruda_vector_at_const(
                    &function->record_fields,
                    record->field_start
                )
                : NULL;
            RudaMirValue *fields = NULL;
            size_t field_bytes = 0;
            size_t argument_end = 0;
            size_t index = 0;
            RudaMirInterpretStatus status;
            if (construction == NULL || record == NULL
                || instruction->record != construction->record
                || options.execution == NULL
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->record_arguments.length
                || !ruda_size_multiply(
                    construction->argument_count,
                    sizeof(RudaMirValue),
                    &field_bytes
                )) {
                return options.execution == NULL
                    ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                    : RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            if (field_bytes != 0) {
                fields = ruda_allocate(allocator, field_bytes);
                if (fields == NULL) {
                    return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
                }
            }
            if (construction->argument_count != 0 && fields == NULL) {
                return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const RudaMirRecordArgument *argument =
                    ruda_mir_record_argument(
                        function,
                        construction->argument_start + index
                    );
                if (argument == NULL
                    || argument->register_id >= register_count) {
                    ruda_deallocate(allocator, fields, field_bytes);
                    return RUDA_MIR_INTERPRET_INVALID_MIR;
                }
                fields[index] = registers[argument->register_id];
            }
            status = ruda_mir_execution_new_record(
                options.execution,
                record->type,
                fields,
                layout_fields,
                construction->argument_count,
                &target->as.reference
            );
            ruda_deallocate(allocator, fields, field_bytes);
            return status;
        }
        case RUDA_MIR_RECORD_GET:
        case RUDA_MIR_RECORD_SET: {
            const RudaMirRecord *layout = ruda_mir_record(
                function,
                instruction->record
            );
            RudaMirManagedRecord *record = options.execution != NULL
                ? ruda_mir_execution_record(
                    options.execution,
                    first->as.reference
                )
                : NULL;
            if (options.execution == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (record == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (layout == NULL || instruction->field >= record->field_count
                || !ruda_mir_managed_record_matches_layout(
                    function,
                    layout,
                    record
                )) {
                return record == NULL
                    ? RUDA_MIR_INTERPRET_INVALID_REFERENCE
                    : RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            if (instruction->kind == RUDA_MIR_RECORD_SET) {
                record->fields[instruction->field] = *second;
                *target = *second;
            } else {
                *target = record->fields[instruction->field];
            }
            return RUDA_MIR_INTERPRET_OK;
        }
        case RUDA_MIR_ENUM_NEW: {
            const RudaMirEnumConstruction *construction =
                ruda_mir_enum_construction(
                    function,
                    instruction->enum_construction
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
            RudaMirValue *payload = NULL;
            size_t payload_bytes = 0;
            size_t argument_end = 0;
            size_t index = 0;
            RudaMirInterpretStatus status;
            if (construction == NULL || enum_layout == NULL || variant == NULL
                || instruction->enum_id != construction->enum_id
                || options.execution == NULL
                || !ruda_size_add(
                    construction->argument_start,
                    construction->argument_count,
                    &argument_end
                ) || argument_end > function->enum_arguments.length
                || !ruda_size_multiply(
                    construction->argument_count,
                    sizeof(RudaMirValue),
                    &payload_bytes
                )) {
                return options.execution == NULL
                    ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                    : RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            if (payload_bytes != 0) {
                payload = ruda_allocate(allocator, payload_bytes);
                if (payload == NULL) {
                    return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
                }
            }
            if (construction->argument_count != 0 && payload == NULL) {
                return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
            }
            for (index = 0; index < construction->argument_count;
                index += 1) {
                const RudaMirEnumArgument *argument = ruda_mir_enum_argument(
                    function,
                    construction->argument_start + index
                );
                if (argument == NULL
                    || argument->register_id >= register_count) {
                    ruda_deallocate(allocator, payload, payload_bytes);
                    return RUDA_MIR_INTERPRET_INVALID_MIR;
                }
                payload[index] = registers[argument->register_id];
            }
            status = ruda_mir_execution_new_enum(
                options.execution,
                function,
                enum_layout,
                construction->variant,
                payload,
                construction->argument_count,
                &target->as.reference
            );
            ruda_deallocate(allocator, payload, payload_bytes);
            return status;
        }
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
        case RUDA_MIR_ENUM_GET_PAYLOAD: {
            const RudaMirEnum *layout = ruda_mir_enum(
                function,
                instruction->enum_id
            );
            const RudaMirEnumVariant *variant = ruda_mir_enum_variant(
                function,
                layout,
                instruction->variant
            );
            const RudaMirManagedEnum *value = options.execution != NULL
                ? ruda_mir_execution_enum_value(options.execution, *first)
                : NULL;
            if (options.execution == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (value == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
            if (layout == NULL || variant == NULL
                || !ruda_mir_managed_enum_matches_layout(
                    function,
                    layout,
                    value
                )) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            if (instruction->kind == RUDA_MIR_ENUM_IS_VARIANT) {
                target->as.boolean = value->variant == instruction->variant;
                return RUDA_MIR_INTERPRET_OK;
            }
            if (value->variant != instruction->variant) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            if (instruction->kind == RUDA_MIR_ENUM_REQUIRE_VARIANT) {
                *target = *first;
                return RUDA_MIR_INTERPRET_OK;
            }
            if (instruction->field >= value->selected_payload_count) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            *target = value->payload[instruction->field];
            return RUDA_MIR_INTERPRET_OK;
        }
        case RUDA_MIR_CALL: {
            const RudaMirCall *call = ruda_mir_call(
                function,
                instruction->call
            );
            RudaMirValue call_result;
            RudaMirInterpretStatus status;
            memset(&call_result, 0, sizeof(call_result));
            status = ruda_mir_invoke_call(
                function,
                instruction->call,
                registers,
                register_count,
                allocator,
                options,
                &call_result
            );
            if (status != RUDA_MIR_INTERPRET_OK) {
                return status;
            }
            if (call == NULL || call_result.type != call->result_type) {
                return RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            *target = call_result;
            return RUDA_MIR_INTERPRET_OK;
        }
    }
    return RUDA_MIR_INTERPRET_INVALID_MIR;
}

RudaMirInterpretStatus ruda_mir_interpret(
    const RudaMirFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaMirInterpretOptions options,
    RudaMirValue *result
) {
    RudaMirVerifyStatus verify_status;
    RudaMirValue *registers = NULL;
    size_t register_bytes = 0;
    size_t register_count = 0;
    size_t index = 0;
    size_t steps = 0;
    RudaMirBlockId current = 0;
    RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_OK;
    if (function == NULL || !function->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || argument_count != function->parameter_count
        || (arguments == NULL && argument_count != 0)
        || options.max_steps == 0) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (options.panic != NULL) {
        ruda_mir_panic_dispose(options.panic);
    }
    verify_status = ruda_mir_verify(function);
    if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    if (ruda_mir_uses_f64(function) && !ruda_f64_binary64_supported()) {
        return RUDA_MIR_INTERPRET_UNSUPPORTED_HOST;
    }
    for (index = 0; index < argument_count; index += 1) {
        if (arguments[index].type != ruda_mir_register_type(
                function,
                (RudaMirRegister)index
            )) {
            return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
        }
        if (arguments[index].type == RUDA_TYPE_STRING
            && ruda_mir_execution_string_value(
                options.execution,
                arguments[index]
            ) == NULL) {
            return options.execution == NULL
                ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                : RUDA_MIR_INTERPRET_INVALID_REFERENCE;
        }
    }
    register_count = function->register_types.length;
    if (!ruda_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(RudaMirValue),
            &register_bytes
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    registers = ruda_allocate(&allocator, register_bytes);
    if (registers == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    memset(registers, 0, register_bytes);
    for (index = 0; index < register_count; index += 1) {
        registers[index].type = ruda_mir_register_type(
            function,
            (RudaMirRegister)index
        );
    }
    for (index = 0; index < argument_count; index += 1) {
        registers[index] = arguments[index];
    }
    for (;;) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            current
        );
        if (block == NULL) {
            status = RUDA_MIR_INTERPRET_INVALID_MIR;
            break;
        }
        for (index = 0; index < block->instructions.length; index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                index
            );
            if (steps >= options.max_steps
                || (options.remaining_shared_steps != NULL
                    && *options.remaining_shared_steps == 0)) {
                status = RUDA_MIR_INTERPRET_STEP_LIMIT;
                goto cleanup;
            }
            steps += 1;
            if (options.remaining_shared_steps != NULL) {
                *options.remaining_shared_steps -= 1;
            }
            status = ruda_mir_execute_instruction(
                function,
                instruction,
                registers,
                register_count,
                &allocator,
                options
            );
            if (status != RUDA_MIR_INTERPRET_OK) {
                goto cleanup;
            }
        }
        if (steps >= options.max_steps
            || (options.remaining_shared_steps != NULL
                && *options.remaining_shared_steps == 0)) {
            status = RUDA_MIR_INTERPRET_STEP_LIMIT;
            break;
        }
        steps += 1;
        if (options.remaining_shared_steps != NULL) {
            *options.remaining_shared_steps -= 1;
        }
        if (block->terminator.kind == RUDA_MIR_RETURN) {
            if (function->result_type == RUDA_TYPE_VOID) {
                memset(result, 0, sizeof(*result));
                result->type = RUDA_TYPE_VOID;
            } else {
                *result = registers[block->terminator.value];
            }
            status = RUDA_MIR_INTERPRET_OK;
            break;
        }
        if (block->terminator.kind == RUDA_MIR_PANIC) {
            const RudaMirManagedString *message =
                ruda_mir_execution_string_value(
                    options.execution,
                    registers[block->terminator.value]
                );
            if (message == NULL) {
                status = options.execution == NULL
                    ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                    : RUDA_MIR_INTERPRET_INVALID_REFERENCE;
                break;
            }
            if (options.panic != NULL) {
                if (!ruda_owned_bytes_copy(
                        &options.panic->message,
                        allocator,
                        message->bytes,
                        message->length
                    )) {
                    status = RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
                    break;
                }
                options.panic->code = "G4000";
                options.panic->symbol = function->symbol;
                options.panic->span = block->terminator.span;
            }
            status = RUDA_MIR_INTERPRET_PANIC;
            break;
        }
        if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            RudaMirValue unexpected_result;
            memset(&unexpected_result, 0, sizeof(unexpected_result));
            status = ruda_mir_invoke_call(
                function,
                block->terminator.call,
                registers,
                register_count,
                &allocator,
                options,
                &unexpected_result
            );
            if (status == RUDA_MIR_INTERPRET_OK) {
                status = RUDA_MIR_INTERPRET_INVALID_MIR;
            }
            break;
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            current = block->terminator.target;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            current = registers[block->terminator.value].as.boolean
                ? block->terminator.target
                : block->terminator.otherwise;
        } else {
            status = RUDA_MIR_INTERPRET_INVALID_MIR;
            break;
        }
    }

cleanup:
    ruda_deallocate(&allocator, registers, register_bytes);
    return status;
}
