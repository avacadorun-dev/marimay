#include "vm_internal.h"

#include <string.h>

typedef struct RudaBytecodeRecordDescriptorCacheEntry {
    RudaSymbolId symbol;
    RudaManagedRecordDescriptor *descriptor;
    bool building;
    bool complete;
} RudaBytecodeRecordDescriptorCacheEntry;

typedef struct RudaBytecodeEnumDescriptorCacheEntry {
    RudaSymbolId symbol;
    RudaManagedEnumDescriptor *descriptor;
    RudaManagedEnumVariantDescriptor *variants;
    RudaManagedFieldDescriptor *payloads;
    size_t variant_bytes;
    size_t payload_bytes;
    bool building;
    bool complete;
} RudaBytecodeEnumDescriptorCacheEntry;

typedef struct RudaBytecodeTemporaryCells {
    RudaManagedCell *cells;
    size_t cell_bytes;
    RudaManagedReference **locations;
    size_t location_bytes;
    RudaManagedRootFrame roots;
    bool rooted;
} RudaBytecodeTemporaryCells;

static void ruda_bytecode_record_cache_entry_dispose(
    RudaBytecodeExecution *execution,
    RudaBytecodeRecordDescriptorCacheEntry *entry
) {
    size_t field_bytes = 0;
    if (entry == NULL || entry->descriptor == NULL) {
        return;
    }
    (void)ruda_size_multiply(
        entry->descriptor->field_count,
        sizeof(RudaManagedFieldDescriptor),
        &field_bytes
    );
    ruda_deallocate(
        &execution->allocator,
        (void *)entry->descriptor->fields,
        field_bytes
    );
    ruda_deallocate(
        &execution->allocator,
        entry->descriptor,
        sizeof(*entry->descriptor)
    );
    memset(entry, 0, sizeof(*entry));
}

static void ruda_bytecode_enum_cache_entry_dispose(
    RudaBytecodeExecution *execution,
    RudaBytecodeEnumDescriptorCacheEntry *entry
) {
    if (entry == NULL) {
        return;
    }
    ruda_deallocate(
        &execution->allocator, entry->payloads, entry->payload_bytes
    );
    ruda_deallocate(
        &execution->allocator, entry->variants, entry->variant_bytes
    );
    ruda_deallocate(
        &execution->allocator,
        entry->descriptor,
        entry->descriptor == NULL ? 0 : sizeof(*entry->descriptor)
    );
    memset(entry, 0, sizeof(*entry));
}

static void ruda_bytecode_nominal_cache_rollback(
    RudaBytecodeExecution *execution,
    size_t record_length,
    size_t enum_length,
    size_t array_length
) {
    ruda_bytecode_array_cache_rollback(execution, array_length);
    while (execution->interned_enum_descriptors.length > enum_length) {
        size_t index = execution->interned_enum_descriptors.length - 1;
        ruda_bytecode_enum_cache_entry_dispose(
            execution,
            ruda_vector_at(&execution->interned_enum_descriptors, index)
        );
        execution->interned_enum_descriptors.length = index;
    }
    while (execution->interned_record_descriptors.length > record_length) {
        size_t index = execution->interned_record_descriptors.length - 1;
        ruda_bytecode_record_cache_entry_dispose(
            execution,
            ruda_vector_at(&execution->interned_record_descriptors, index)
        );
        execution->interned_record_descriptors.length = index;
    }
}

static bool ruda_bytecode_symbol_equal(
    RudaSymbolId left,
    RudaSymbolId right
) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

bool ruda_bytecode_nominal_cache_init(
    RudaBytecodeExecution *execution,
    RudaAllocator allocator
) {
    if (execution == NULL) {
        return false;
    }
    if (!ruda_vector_init(
            &execution->interned_record_descriptors,
            allocator,
            sizeof(RudaBytecodeRecordDescriptorCacheEntry)
        ) || !ruda_vector_init(
            &execution->interned_record_fields,
            allocator,
            sizeof(RudaManagedFieldDescriptor)
        ) || !ruda_vector_init(
            &execution->interned_enum_descriptors,
            allocator,
            sizeof(RudaBytecodeEnumDescriptorCacheEntry)
        ) || !ruda_vector_init(
            &execution->interned_enum_variants,
            allocator,
            sizeof(RudaManagedEnumVariantDescriptor)
        ) || !ruda_vector_init(
            &execution->interned_enum_payloads,
            allocator,
            sizeof(RudaManagedFieldDescriptor)
        )) {
        ruda_bytecode_nominal_cache_dispose(execution);
        return false;
    }
    return true;
}

void ruda_bytecode_nominal_cache_dispose(
    RudaBytecodeExecution *execution
) {
    size_t index = 0;
    if (execution == NULL) {
        return;
    }
    for (index = 0;
        index < execution->interned_record_descriptors.length;
        index += 1) {
        ruda_bytecode_record_cache_entry_dispose(
            execution,
            ruda_vector_at(&execution->interned_record_descriptors, index)
        );
    }
    for (index = 0;
        index < execution->interned_enum_descriptors.length;
        index += 1) {
        ruda_bytecode_enum_cache_entry_dispose(
            execution,
            ruda_vector_at(&execution->interned_enum_descriptors, index)
        );
    }
    ruda_vector_dispose(&execution->interned_record_descriptors);
    ruda_vector_dispose(&execution->interned_record_fields);
    ruda_vector_dispose(&execution->interned_enum_descriptors);
    ruda_vector_dispose(&execution->interned_enum_variants);
    ruda_vector_dispose(&execution->interned_enum_payloads);
}

static RudaManagedCellKind ruda_bytecode_nominal_cell_kind(
    RudaBytecodeType type
) {
    return ruda_bytecode_type_is_integer(type)
            || type == RUDA_BYTECODE_TYPE_BOOL || type == RUDA_BYTECODE_TYPE_F64
            || type == RUDA_BYTECODE_TYPE_VOID
        ? RUDA_MANAGED_CELL_RAW64
        : RUDA_MANAGED_CELL_REFERENCE;
}

RudaManagedObjectKind ruda_bytecode_nominal_object_kind(
    RudaBytecodeType type
) {
    if (type == RUDA_BYTECODE_TYPE_STRING) {
        return RUDA_MANAGED_OBJECT_STRING;
    }
    if (type == RUDA_BYTECODE_TYPE_RECORD) {
        return RUDA_MANAGED_OBJECT_RECORD;
    }
    if (type == RUDA_BYTECODE_TYPE_ENUM) {
        return RUDA_MANAGED_OBJECT_ENUM;
    }
    if (type == RUDA_BYTECODE_TYPE_INTERFACE) {
        return RUDA_MANAGED_OBJECT_INTERFACE;
    }
    if (ruda_bytecode_type_is_array(type)) {
        return RUDA_MANAGED_OBJECT_ARRAY;
    }
    return RUDA_MANAGED_OBJECT_INVALID;
}

static const RudaManagedRecordDescriptor *ruda_bytecode_record_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeRecordId record
);

static const RudaManagedEnumDescriptor *ruda_bytecode_enum_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeEnumId enum_id
);

const void *ruda_bytecode_nominal_reference_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t layout
) {
    if (type == RUDA_BYTECODE_TYPE_RECORD
        && layout != RUDA_BYTECODE_NO_LAYOUT) {
        return ruda_bytecode_record_descriptor(execution, function, layout);
    }
    if (type == RUDA_BYTECODE_TYPE_ENUM
        && layout != RUDA_BYTECODE_NO_LAYOUT) {
        return ruda_bytecode_enum_descriptor(execution, function, layout);
    }
    if (type == RUDA_BYTECODE_TYPE_INTERFACE
        && layout != RUDA_BYTECODE_NO_LAYOUT) {
        return ruda_bytecode_run_interface_descriptor(
            execution,
            function,
            layout
        );
    }
    return ruda_bytecode_array_descriptor_for(execution, function, type, layout);
}

static bool ruda_bytecode_nominal_field_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t layout,
    bool is_mutable,
    RudaManagedFieldDescriptor *output
) {
    memset(output, 0, sizeof(*output));
    output->cell_kind = ruda_bytecode_nominal_cell_kind(type);
    output->reference_kind = ruda_bytecode_nominal_object_kind(type);
    output->is_mutable = is_mutable;
    if (output->cell_kind == RUDA_MANAGED_CELL_RAW64) {
        return output->reference_kind == RUDA_MANAGED_OBJECT_INVALID;
    }
    output->reference_descriptor = ruda_bytecode_nominal_reference_descriptor(
        execution, function, type, layout
    );
    return type == RUDA_BYTECODE_TYPE_STRING
        || output->reference_descriptor != NULL;
}

static RudaBytecodeRecordDescriptorCacheEntry *
ruda_bytecode_find_record_descriptor(
    RudaBytecodeExecution *execution,
    RudaSymbolId symbol
) {
    size_t index = 0;
    for (index = 0;
        index < execution->interned_record_descriptors.length;
        index += 1) {
        RudaBytecodeRecordDescriptorCacheEntry *entry = ruda_vector_at(
            &execution->interned_record_descriptors, index
        );
        if (entry != NULL && ruda_bytecode_symbol_equal(entry->symbol, symbol)) {
            return entry;
        }
    }
    return NULL;
}

static RudaBytecodeEnumDescriptorCacheEntry *
ruda_bytecode_find_enum_descriptor(
    RudaBytecodeExecution *execution,
    RudaSymbolId symbol
) {
    size_t index = 0;
    for (index = 0;
        index < execution->interned_enum_descriptors.length;
        index += 1) {
        RudaBytecodeEnumDescriptorCacheEntry *entry = ruda_vector_at(
            &execution->interned_enum_descriptors, index
        );
        if (entry != NULL && ruda_bytecode_symbol_equal(entry->symbol, symbol)) {
            return entry;
        }
    }
    return NULL;
}

static const RudaManagedRecordDescriptor *ruda_bytecode_record_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeRecordId record
) {
    const RudaBytecodeRecordLayout *layout = ruda_bytecode_record_layout(
        function, record
    );
    RudaBytecodeRecordDescriptorCacheEntry *existing = NULL;
    RudaBytecodeRecordDescriptorCacheEntry entry;
    RudaManagedRecordDescriptor *descriptor = NULL;
    RudaManagedFieldDescriptor *fields = NULL;
    size_t record_cache_start = 0;
    size_t enum_cache_start = 0;
    size_t array_cache_start = 0;
    size_t entry_index = 0;
    size_t field_bytes = 0;
    size_t field_index = 0;
    if (execution == NULL || layout == NULL || !ruda_size_multiply(
            layout->field_count, sizeof(*fields), &field_bytes
        )) {
        return NULL;
    }
    existing = ruda_bytecode_find_record_descriptor(execution, layout->symbol);
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor
            : NULL;
    }
    record_cache_start = execution->interned_record_descriptors.length;
    enum_cache_start = execution->interned_enum_descriptors.length;
    array_cache_start = execution->array_descriptors.length;
    descriptor = ruda_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    if (field_bytes != 0) {
        fields = ruda_allocate(&execution->allocator, field_bytes);
        if (fields == NULL) {
            ruda_deallocate(
                &execution->allocator, descriptor, sizeof(*descriptor)
            );
            return NULL;
        }
        memset(fields, 0, field_bytes);
    }
    descriptor->field_count = layout->field_count;
    descriptor->fields = fields;
    memset(&entry, 0, sizeof(entry));
    entry.symbol = layout->symbol;
    entry.descriptor = descriptor;
    entry.building = true;
    if (!ruda_vector_push(&execution->interned_record_descriptors, &entry)) {
        ruda_deallocate(&execution->allocator, fields, field_bytes);
        ruda_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    entry_index = execution->interned_record_descriptors.length - 1;
    for (field_index = 0; field_index < layout->field_count;
        field_index += 1) {
        const RudaBytecodeRecordField *field = ruda_bytecode_record_field(
            function, layout, field_index
        );
        if (field == NULL || !ruda_bytecode_nominal_field_descriptor(
                execution, function, field->type, field->layout,
                field->is_mutable, &fields[field_index]
            )) {
            ruda_bytecode_nominal_cache_rollback(
                execution, record_cache_start, enum_cache_start,
                array_cache_start
            );
            return NULL;
        }
    }
    existing = ruda_vector_at(
        &execution->interned_record_descriptors, entry_index
    );
    if (existing == NULL) {
        ruda_bytecode_nominal_cache_rollback(
            execution, record_cache_start, enum_cache_start,
            array_cache_start
        );
        return NULL;
    }
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

static const RudaManagedEnumDescriptor *ruda_bytecode_enum_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeEnumId enum_id
) {
    const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
        function, enum_id
    );
    RudaBytecodeEnumDescriptorCacheEntry *existing = NULL;
    RudaBytecodeEnumDescriptorCacheEntry entry;
    RudaManagedEnumDescriptor *descriptor = NULL;
    RudaManagedEnumVariantDescriptor *variants = NULL;
    RudaManagedFieldDescriptor *payloads = NULL;
    size_t record_cache_start = 0;
    size_t enum_cache_start = 0;
    size_t array_cache_start = 0;
    size_t entry_index = 0;
    size_t variant_bytes = 0;
    size_t payload_bytes = 0;
    size_t variant_index = 0;
    size_t payload_index = 0;
    if (execution == NULL || layout == NULL || !ruda_size_multiply(
            layout->variant_count, sizeof(*variants), &variant_bytes
        ) || !ruda_size_multiply(
            layout->payload_count, sizeof(*payloads), &payload_bytes
        )) {
        return NULL;
    }
    existing = ruda_bytecode_find_enum_descriptor(execution, layout->symbol);
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor
            : NULL;
    }
    record_cache_start = execution->interned_record_descriptors.length;
    enum_cache_start = execution->interned_enum_descriptors.length;
    array_cache_start = execution->array_descriptors.length;
    descriptor = ruda_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    if (variant_bytes != 0) {
        variants = ruda_allocate(&execution->allocator, variant_bytes);
    }
    if (payload_bytes != 0) {
        payloads = ruda_allocate(&execution->allocator, payload_bytes);
    }
    if ((variant_bytes != 0 && variants == NULL)
        || (payload_bytes != 0 && payloads == NULL)) {
        ruda_deallocate(&execution->allocator, payloads, payload_bytes);
        ruda_deallocate(&execution->allocator, variants, variant_bytes);
        ruda_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    if (variant_bytes != 0) {
        memset(variants, 0, variant_bytes);
    }
    if (payload_bytes != 0) {
        memset(payloads, 0, payload_bytes);
    }
    descriptor->variant_count = layout->variant_count;
    descriptor->variants = variants;
    memset(&entry, 0, sizeof(entry));
    entry.symbol = layout->symbol;
    entry.descriptor = descriptor;
    entry.variants = variants;
    entry.payloads = payloads;
    entry.variant_bytes = variant_bytes;
    entry.payload_bytes = payload_bytes;
    entry.building = true;
    if (!ruda_vector_push(&execution->interned_enum_descriptors, &entry)) {
        ruda_deallocate(&execution->allocator, payloads, payload_bytes);
        ruda_deallocate(&execution->allocator, variants, variant_bytes);
        ruda_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    entry_index = execution->interned_enum_descriptors.length - 1;
    for (payload_index = 0; payload_index < layout->payload_count;
        payload_index += 1) {
        const RudaBytecodeEnumPayload *payload = ruda_vector_at_const(
            &function->enum_payloads, layout->payload_start + payload_index
        );
        if (payload == NULL || !ruda_bytecode_nominal_field_descriptor(
                execution, function, payload->type, payload->layout, false,
                &payloads[payload_index]
            )) {
            ruda_bytecode_nominal_cache_rollback(
                execution, record_cache_start, enum_cache_start,
                array_cache_start
            );
            return NULL;
        }
    }
    for (variant_index = 0; variant_index < layout->variant_count;
        variant_index += 1) {
        const RudaBytecodeEnumVariant *variant = ruda_vector_at_const(
            &function->enum_variants, layout->variant_start + variant_index
        );
        if (variant == NULL || variant->payload_start > layout->payload_count
            || variant->payload_count
                > layout->payload_count - variant->payload_start) {
            ruda_bytecode_nominal_cache_rollback(
                execution, record_cache_start, enum_cache_start,
                array_cache_start
            );
            return NULL;
        }
        variants[variant_index].payload_count = variant->payload_count;
        variants[variant_index].payloads = variant->payload_count == 0
            ? NULL
            : &payloads[variant->payload_start];
    }
    existing = ruda_vector_at(
        &execution->interned_enum_descriptors, entry_index
    );
    if (existing == NULL) {
        ruda_bytecode_nominal_cache_rollback(
            execution, record_cache_start, enum_cache_start,
            array_cache_start
        );
        return NULL;
    }
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

RudaBytecodeRunStatus ruda_bytecode_run_nominal_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t layout,
    const void **descriptor
) {
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (execution == NULL || function == NULL || descriptor == NULL
        || (type != RUDA_BYTECODE_TYPE_RECORD
            && type != RUDA_BYTECODE_TYPE_ENUM)
        || layout == RUDA_BYTECODE_NO_LAYOUT
        || (type == RUDA_BYTECODE_TYPE_RECORD
            && ruda_bytecode_record_layout(function, layout) == NULL)
        || (type == RUDA_BYTECODE_TYPE_ENUM
            && ruda_bytecode_enum_layout(function, layout) == NULL)) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    *descriptor = ruda_bytecode_nominal_reference_descriptor(
        execution, function, type, layout
    );
    return *descriptor != NULL
        ? RUDA_BYTECODE_RUN_OK
        : RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
}

RudaBytecodeRunStatus ruda_bytecode_run_nominal_value_validate(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue value,
    RudaBytecodeType type,
    uint32_t layout
) {
    const void *expected = NULL;
    const void *actual = NULL;
    size_t variant = 0;
    RudaBytecodeRunStatus status = ruda_bytecode_run_nominal_descriptor(
        execution, function, type, layout, &expected
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    if (value.type != type) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (type == RUDA_BYTECODE_TYPE_RECORD) {
        const RudaManagedRecordDescriptor *record = NULL;
        if (ruda_managed_record_describe(
                &execution->managed_heap,
                value.as.managed_reference,
                &record
            ) == RUDA_MANAGED_OK) {
            actual = record;
        }
    } else {
        const RudaManagedEnumDescriptor *enumeration = NULL;
        if (ruda_managed_enum_describe(
                &execution->managed_heap,
                value.as.managed_reference,
                &enumeration,
                &variant
            ) == RUDA_MANAGED_OK) {
            actual = enumeration;
        }
    }
    return actual == expected
        ? RUDA_BYTECODE_RUN_OK
        : RUDA_BYTECODE_RUN_INVALID_REFERENCE;
}

RudaBytecodeRunStatus ruda_bytecode_value_to_managed_cell(
    RudaBytecodeExecution *execution,
    const RudaBytecodeValue *value,
    RudaManagedCell *cell
) {
    memset(cell, 0, sizeof(*cell));
    if (ruda_bytecode_type_is_integer(value->type)) {
        cell->f64_bits = value->as.integer_bits;
    } else if (value->type == RUDA_BYTECODE_TYPE_BOOL) {
        cell->f64_bits = value->as.boolean ? 1 : 0;
    } else if (value->type == RUDA_BYTECODE_TYPE_F64) {
        cell->f64_bits = value->as.f64_bits;
    } else if (value->type == RUDA_BYTECODE_TYPE_F32) {
        cell->f64_bits = value->as.integer_bits;
    } else if (value->type == RUDA_BYTECODE_TYPE_STRING) {
        const unsigned char *bytes = NULL;
        size_t length = 0;
        RudaManagedStatus status;
        if (!ruda_bytecode_execution_string_bytes(
                execution, *value, &bytes, &length
            )) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
        }
        status = ruda_managed_string_new(
            &execution->managed_heap, bytes, length, &cell->reference
        );
        return ruda_bytecode_run_managed_status(status);
    } else {
        cell->reference = value->as.managed_reference;
    }
    return RUDA_BYTECODE_RUN_OK;
}

RudaBytecodeRunStatus ruda_bytecode_managed_cell_to_value(
    RudaBytecodeExecution *execution,
    RudaManagedCell cell,
    RudaBytecodeValue *value
) {
    if (ruda_bytecode_type_is_integer(value->type)) {
        value->as.integer_bits = cell.f64_bits;
    } else if (value->type == RUDA_BYTECODE_TYPE_BOOL) {
        value->as.boolean = cell.f64_bits != 0;
    } else if (value->type == RUDA_BYTECODE_TYPE_F64) {
        value->as.f64_bits = cell.f64_bits;
    } else if (value->type == RUDA_BYTECODE_TYPE_F32) {
        value->as.integer_bits = cell.f64_bits;
    } else if (value->type == RUDA_BYTECODE_TYPE_STRING) {
        const unsigned char *bytes = NULL;
        size_t length = 0;
        if (ruda_managed_string_bytes(
                &execution->managed_heap, cell.reference, &bytes, &length
            ) != RUDA_MANAGED_OK) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
        }
        return ruda_bytecode_execution_intern(
            execution, bytes, length, &value->as.reference
        );
    } else {
        value->as.managed_reference = cell.reference;
    }
    return RUDA_BYTECODE_RUN_OK;
}

RudaBytecodeRunStatus ruda_bytecode_execution_new_numeric_option(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    uint32_t enum_id,
    bool some,
    RudaBytecodeValue payload,
    RudaBytecodeValue *target
) {
    const RudaManagedEnumDescriptor *descriptor = execution != NULL
        ? ruda_bytecode_enum_descriptor(execution, function, enum_id)
        : NULL;
    RudaManagedCell cell;
    RudaBytecodeRunStatus status;
    if (execution == NULL || descriptor == NULL || target == NULL) {
        return execution == NULL
            ? RUDA_BYTECODE_RUN_INVALID_ARGUMENT
            : RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(&cell, 0, sizeof(cell));
    if (some) {
        status = ruda_bytecode_value_to_managed_cell(
            execution, &payload, &cell
        );
        if (status != RUDA_BYTECODE_RUN_OK) {
            return status;
        }
    }
    return ruda_bytecode_run_managed_status(ruda_managed_enum_new(
        &execution->managed_heap,
        descriptor,
        some ? 0 : 1,
        some ? &cell : NULL,
        some ? 1 : 0,
        &target->as.managed_reference
    ));
}

static void ruda_bytecode_temporary_cells_dispose(
    RudaBytecodeTemporaryCells *temporary,
    RudaBytecodeExecution *execution,
    RudaAllocator allocator
) {
    if (temporary->rooted) {
        (void)ruda_managed_root_pop(
            &execution->managed_heap, &temporary->roots
        );
    }
    ruda_deallocate(
        &allocator, temporary->locations, temporary->location_bytes
    );
    ruda_deallocate(&allocator, temporary->cells, temporary->cell_bytes);
    memset(temporary, 0, sizeof(*temporary));
}

static RudaBytecodeRunStatus ruda_bytecode_prepare_temporary_cells(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue *registers,
    bool is_record,
    size_t argument_start,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeTemporaryCells *temporary
) {
    size_t index = 0;
    size_t string_count = 0;
    size_t string_index = 0;
    RudaManagedStatus root_status;
    memset(temporary, 0, sizeof(*temporary));
    if (!ruda_size_multiply(
            argument_count, sizeof(*temporary->cells), &temporary->cell_bytes
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType type = is_record
            ? ((const RudaBytecodeRecordArgument *)ruda_vector_at_const(
                &function->record_arguments, argument_start + index
            ))->type
            : ((const RudaBytecodeEnumArgument *)ruda_vector_at_const(
                &function->enum_arguments, argument_start + index
            ))->type;
        if (type == RUDA_BYTECODE_TYPE_STRING) {
            string_count += 1;
        }
    }
    if (!ruda_size_multiply(
            string_count,
            sizeof(*temporary->locations),
            &temporary->location_bytes
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (temporary->cell_bytes != 0) {
        temporary->cells = ruda_allocate(
            &allocator, temporary->cell_bytes
        );
    }
    if (temporary->location_bytes != 0) {
        temporary->locations = ruda_allocate(
            &allocator, temporary->location_bytes
        );
    }
    if ((temporary->cell_bytes != 0 && temporary->cells == NULL)
        || (temporary->location_bytes != 0
            && temporary->locations == NULL)) {
        ruda_bytecode_temporary_cells_dispose(
            temporary, execution, allocator
        );
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (temporary->cell_bytes != 0) {
        memset(temporary->cells, 0, temporary->cell_bytes);
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType type = is_record
            ? ((const RudaBytecodeRecordArgument *)ruda_vector_at_const(
                &function->record_arguments, argument_start + index
            ))->type
            : ((const RudaBytecodeEnumArgument *)ruda_vector_at_const(
                &function->enum_arguments, argument_start + index
            ))->type;
        if (type == RUDA_BYTECODE_TYPE_STRING) {
            temporary->cells[index].reference =
                ruda_managed_invalid_reference();
            temporary->locations[string_index] =
                &temporary->cells[index].reference;
            string_index += 1;
        }
    }
    root_status = ruda_managed_root_push(
        &execution->managed_heap,
        &temporary->roots,
        temporary->locations,
        string_count
    );
    if (root_status != RUDA_MANAGED_OK) {
        ruda_bytecode_temporary_cells_dispose(
            temporary, execution, allocator
        );
        return ruda_bytecode_run_managed_status(root_status);
    }
    temporary->rooted = true;
    for (index = 0; index < argument_count; index += 1) {
        uint32_t register_id = is_record
            ? ((const RudaBytecodeRecordArgument *)ruda_vector_at_const(
                &function->record_arguments, argument_start + index
            ))->register_id
            : ((const RudaBytecodeEnumArgument *)ruda_vector_at_const(
                &function->enum_arguments, argument_start + index
            ))->register_id;
        RudaBytecodeRunStatus status = ruda_bytecode_value_to_managed_cell(
            execution, &registers[register_id], &temporary->cells[index]
        );
        if (status != RUDA_BYTECODE_RUN_OK) {
            ruda_bytecode_temporary_cells_dispose(
                temporary, execution, allocator
            );
            return status;
        }
    }
    return RUDA_BYTECODE_RUN_OK;
}

RudaBytecodeRunStatus ruda_bytecode_run_nominal_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    if (execution == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    switch (instruction->opcode) {
        case RUDA_OPCODE_RECORD_NEW: {
            const RudaBytecodeRecordConstruction *construction =
                ruda_bytecode_record_construction(
                    function,
                    (RudaBytecodeRecordConstructionId)
                        instruction->operands[1].unsigned_value
                );
            const RudaManagedRecordDescriptor *descriptor = construction != NULL
                ? ruda_bytecode_record_descriptor(
                    execution, function, construction->record
                )
                : NULL;
            RudaBytecodeTemporaryCells temporary;
            RudaManagedStatus managed_status;
            RudaBytecodeRunStatus status;
            if (construction == NULL || descriptor == NULL) {
                return construction == NULL
                    ? RUDA_BYTECODE_RUN_INVALID_BYTECODE
                    : RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            status = ruda_bytecode_prepare_temporary_cells(
                execution, function, registers, true,
                construction->argument_start, construction->argument_count,
                allocator, &temporary
            );
            if (status != RUDA_BYTECODE_RUN_OK) {
                return status;
            }
            managed_status = ruda_managed_record_new(
                &execution->managed_heap,
                descriptor,
                temporary.cells,
                construction->argument_count,
                &registers[target].as.managed_reference
            );
            ruda_bytecode_temporary_cells_dispose(
                &temporary, execution, allocator
            );
            return ruda_bytecode_run_managed_status(managed_status);
        }
        case RUDA_OPCODE_RECORD_GET: {
            RudaManagedCell cell;
            RudaManagedStatus managed_status = ruda_managed_record_get(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                &cell
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_run_managed_status(managed_status);
            }
            return ruda_bytecode_managed_cell_to_value(
                execution, cell, &registers[target]
            );
        }
        case RUDA_OPCODE_RECORD_SET: {
            RudaManagedCell cell;
            RudaBytecodeRunStatus status = ruda_bytecode_value_to_managed_cell(
                execution, &registers[target], &cell
            );
            if (status != RUDA_BYTECODE_RUN_OK) {
                return status;
            }
            return ruda_bytecode_run_managed_status(ruda_managed_record_set(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                cell
            ));
        }
        case RUDA_OPCODE_RECORD_IDENTITY_SAME:
            registers[target].as.boolean = ruda_managed_reference_equal(
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                registers[(size_t)instruction->operands[2].unsigned_value]
                    .as.managed_reference
            );
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_ENUM_NEW: {
            const RudaBytecodeEnumConstruction *construction =
                ruda_bytecode_enum_construction(
                    function,
                    (RudaBytecodeEnumConstructionId)
                        instruction->operands[1].unsigned_value
                );
            const RudaManagedEnumDescriptor *descriptor = construction != NULL
                ? ruda_bytecode_enum_descriptor(
                    execution, function, construction->enum_id
                )
                : NULL;
            RudaBytecodeTemporaryCells temporary;
            RudaManagedStatus managed_status;
            RudaBytecodeRunStatus status;
            if (construction == NULL || descriptor == NULL) {
                return construction == NULL
                    ? RUDA_BYTECODE_RUN_INVALID_BYTECODE
                    : RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            status = ruda_bytecode_prepare_temporary_cells(
                execution, function, registers, false,
                construction->argument_start, construction->argument_count,
                allocator, &temporary
            );
            if (status != RUDA_BYTECODE_RUN_OK) {
                return status;
            }
            managed_status = ruda_managed_enum_new(
                &execution->managed_heap,
                descriptor,
                construction->variant,
                temporary.cells,
                construction->argument_count,
                &registers[target].as.managed_reference
            );
            ruda_bytecode_temporary_cells_dispose(
                &temporary, execution, allocator
            );
            return ruda_bytecode_run_managed_status(managed_status);
        }
        case RUDA_OPCODE_ENUM_IS_VARIANT: {
            bool matches = false;
            RudaManagedStatus managed_status = ruda_managed_enum_is_variant(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                &matches
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_run_managed_status(managed_status);
            }
            registers[target].as.boolean = matches;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_ENUM_REQUIRE_VARIANT: {
            bool matches = false;
            size_t input = (size_t)instruction->operands[1].unsigned_value;
            RudaManagedStatus managed_status = ruda_managed_enum_is_variant(
                &execution->managed_heap,
                registers[input].as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                &matches
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_run_managed_status(managed_status);
            }
            if (!matches) {
                return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            }
            registers[target] = registers[input];
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_ENUM_GET_PAYLOAD: {
            uint64_t selector =
                (uint64_t)instruction->operands[2].signed_value;
            RudaManagedCell cell;
            RudaManagedStatus managed_status = ruda_managed_enum_get_payload(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)(selector >> 32),
                (size_t)(selector & UINT32_MAX),
                &cell
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_run_managed_status(managed_status);
            }
            return ruda_bytecode_managed_cell_to_value(
                execution, cell, &registers[target]
            );
        }
        default:
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
}
