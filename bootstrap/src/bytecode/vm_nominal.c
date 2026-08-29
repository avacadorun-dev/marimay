#include "vm_internal.h"

#include <string.h>

typedef struct MarimayBytecodeRecordDescriptorCacheEntry {
    MarimaySymbolId symbol;
    MarimayManagedRecordDescriptor *descriptor;
    bool building;
    bool complete;
} MarimayBytecodeRecordDescriptorCacheEntry;

typedef struct MarimayBytecodeEnumDescriptorCacheEntry {
    MarimaySymbolId symbol;
    MarimayManagedEnumDescriptor *descriptor;
    MarimayManagedEnumVariantDescriptor *variants;
    MarimayManagedFieldDescriptor *payloads;
    size_t variant_bytes;
    size_t payload_bytes;
    bool building;
    bool complete;
} MarimayBytecodeEnumDescriptorCacheEntry;

typedef struct MarimayBytecodeTemporaryCells {
    MarimayManagedCell *cells;
    size_t cell_bytes;
    MarimayManagedReference **locations;
    size_t location_bytes;
    MarimayManagedRootFrame roots;
    bool rooted;
} MarimayBytecodeTemporaryCells;

static void mmay_bytecode_record_cache_entry_dispose(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeRecordDescriptorCacheEntry *entry
) {
    size_t field_bytes = 0;
    if (entry == NULL || entry->descriptor == NULL) {
        return;
    }
    (void)mmay_size_multiply(
        entry->descriptor->field_count,
        sizeof(MarimayManagedFieldDescriptor),
        &field_bytes
    );
    mmay_deallocate(
        &execution->allocator,
        (void *)entry->descriptor->fields,
        field_bytes
    );
    mmay_deallocate(
        &execution->allocator,
        entry->descriptor,
        sizeof(*entry->descriptor)
    );
    memset(entry, 0, sizeof(*entry));
}

static void mmay_bytecode_enum_cache_entry_dispose(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeEnumDescriptorCacheEntry *entry
) {
    if (entry == NULL) {
        return;
    }
    mmay_deallocate(
        &execution->allocator, entry->payloads, entry->payload_bytes
    );
    mmay_deallocate(
        &execution->allocator, entry->variants, entry->variant_bytes
    );
    mmay_deallocate(
        &execution->allocator,
        entry->descriptor,
        entry->descriptor == NULL ? 0 : sizeof(*entry->descriptor)
    );
    memset(entry, 0, sizeof(*entry));
}

static void mmay_bytecode_nominal_cache_rollback(
    MarimayBytecodeExecution *execution,
    size_t record_length,
    size_t enum_length,
    size_t array_length
) {
    mmay_bytecode_array_cache_rollback(execution, array_length);
    while (execution->interned_enum_descriptors.length > enum_length) {
        size_t index = execution->interned_enum_descriptors.length - 1;
        mmay_bytecode_enum_cache_entry_dispose(
            execution,
            mmay_vector_at(&execution->interned_enum_descriptors, index)
        );
        execution->interned_enum_descriptors.length = index;
    }
    while (execution->interned_record_descriptors.length > record_length) {
        size_t index = execution->interned_record_descriptors.length - 1;
        mmay_bytecode_record_cache_entry_dispose(
            execution,
            mmay_vector_at(&execution->interned_record_descriptors, index)
        );
        execution->interned_record_descriptors.length = index;
    }
}

static bool mmay_bytecode_symbol_equal(
    MarimaySymbolId left,
    MarimaySymbolId right
) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

bool mmay_bytecode_nominal_cache_init(
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    if (execution == NULL) {
        return false;
    }
    if (!mmay_vector_init(
            &execution->interned_record_descriptors,
            allocator,
            sizeof(MarimayBytecodeRecordDescriptorCacheEntry)
        ) || !mmay_vector_init(
            &execution->interned_record_fields,
            allocator,
            sizeof(MarimayManagedFieldDescriptor)
        ) || !mmay_vector_init(
            &execution->interned_enum_descriptors,
            allocator,
            sizeof(MarimayBytecodeEnumDescriptorCacheEntry)
        ) || !mmay_vector_init(
            &execution->interned_enum_variants,
            allocator,
            sizeof(MarimayManagedEnumVariantDescriptor)
        ) || !mmay_vector_init(
            &execution->interned_enum_payloads,
            allocator,
            sizeof(MarimayManagedFieldDescriptor)
        )) {
        mmay_bytecode_nominal_cache_dispose(execution);
        return false;
    }
    return true;
}

void mmay_bytecode_nominal_cache_dispose(
    MarimayBytecodeExecution *execution
) {
    size_t index = 0;
    if (execution == NULL) {
        return;
    }
    for (index = 0;
        index < execution->interned_record_descriptors.length;
        index += 1) {
        mmay_bytecode_record_cache_entry_dispose(
            execution,
            mmay_vector_at(&execution->interned_record_descriptors, index)
        );
    }
    for (index = 0;
        index < execution->interned_enum_descriptors.length;
        index += 1) {
        mmay_bytecode_enum_cache_entry_dispose(
            execution,
            mmay_vector_at(&execution->interned_enum_descriptors, index)
        );
    }
    mmay_vector_dispose(&execution->interned_record_descriptors);
    mmay_vector_dispose(&execution->interned_record_fields);
    mmay_vector_dispose(&execution->interned_enum_descriptors);
    mmay_vector_dispose(&execution->interned_enum_variants);
    mmay_vector_dispose(&execution->interned_enum_payloads);
}

static MarimayManagedCellKind mmay_bytecode_nominal_cell_kind(
    MarimayBytecodeType type
) {
    return mmay_bytecode_type_is_integer(type)
            || type == MMAY_BYTECODE_TYPE_BOOL || type == MMAY_BYTECODE_TYPE_F64
            || type == MMAY_BYTECODE_TYPE_RUNE
            || type == MMAY_BYTECODE_TYPE_VOID
        ? MMAY_MANAGED_CELL_RAW64
        : MMAY_MANAGED_CELL_REFERENCE;
}

MarimayManagedObjectKind mmay_bytecode_nominal_object_kind(
    MarimayBytecodeType type
) {
    if (type == MMAY_BYTECODE_TYPE_STRING) {
        return MMAY_MANAGED_OBJECT_STRING;
    }
    if (type == MMAY_BYTECODE_TYPE_RECORD) {
        return MMAY_MANAGED_OBJECT_RECORD;
    }
    if (type == MMAY_BYTECODE_TYPE_ENUM) {
        return MMAY_MANAGED_OBJECT_ENUM;
    }
    if (type == MMAY_BYTECODE_TYPE_INTERFACE) {
        return MMAY_MANAGED_OBJECT_INTERFACE;
    }
    if (mmay_bytecode_type_is_array(type)) {
        return MMAY_MANAGED_OBJECT_ARRAY;
    }
    if (type == MMAY_BYTECODE_TYPE_MAP || type == MMAY_BYTECODE_TYPE_SET) {
        return MMAY_MANAGED_OBJECT_MAP;
    }
    return MMAY_MANAGED_OBJECT_INVALID;
}

static const MarimayManagedRecordDescriptor *mmay_bytecode_record_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeRecordId record
);

static const MarimayManagedEnumDescriptor *mmay_bytecode_enum_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEnumId enum_id
);

const void *mmay_bytecode_nominal_reference_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout
) {
    if ((type == MMAY_BYTECODE_TYPE_MAP
            || type == MMAY_BYTECODE_TYPE_SET)
        && layout != MMAY_BYTECODE_NO_LAYOUT) {
        return mmay_bytecode_collection_descriptor_for(
            execution, function, layout
        );
    }
    if (type == MMAY_BYTECODE_TYPE_RECORD
        && layout != MMAY_BYTECODE_NO_LAYOUT) {
        return mmay_bytecode_record_descriptor(execution, function, layout);
    }
    if (type == MMAY_BYTECODE_TYPE_ENUM
        && layout != MMAY_BYTECODE_NO_LAYOUT) {
        return mmay_bytecode_enum_descriptor(execution, function, layout);
    }
    if (type == MMAY_BYTECODE_TYPE_INTERFACE
        && layout != MMAY_BYTECODE_NO_LAYOUT) {
        return mmay_bytecode_run_interface_descriptor(
            execution,
            function,
            layout
        );
    }
    return mmay_bytecode_array_descriptor_for(execution, function, type, layout);
}

bool mmay_bytecode_managed_field_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout,
    bool is_mutable,
    MarimayManagedFieldDescriptor *output
) {
    memset(output, 0, sizeof(*output));
    output->cell_kind = mmay_bytecode_nominal_cell_kind(type);
    output->reference_kind = mmay_bytecode_nominal_object_kind(type);
    output->is_mutable = is_mutable;
    if (output->cell_kind == MMAY_MANAGED_CELL_RAW64) {
        return output->reference_kind == MMAY_MANAGED_OBJECT_INVALID;
    }
    output->reference_descriptor = mmay_bytecode_nominal_reference_descriptor(
        execution, function, type, layout
    );
    return type == MMAY_BYTECODE_TYPE_STRING
        || output->reference_descriptor != NULL;
}

static MarimayBytecodeRecordDescriptorCacheEntry *
mmay_bytecode_find_record_descriptor(
    MarimayBytecodeExecution *execution,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0;
        index < execution->interned_record_descriptors.length;
        index += 1) {
        MarimayBytecodeRecordDescriptorCacheEntry *entry = mmay_vector_at(
            &execution->interned_record_descriptors, index
        );
        if (entry != NULL && mmay_bytecode_symbol_equal(entry->symbol, symbol)) {
            return entry;
        }
    }
    return NULL;
}

static MarimayBytecodeEnumDescriptorCacheEntry *
mmay_bytecode_find_enum_descriptor(
    MarimayBytecodeExecution *execution,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0;
        index < execution->interned_enum_descriptors.length;
        index += 1) {
        MarimayBytecodeEnumDescriptorCacheEntry *entry = mmay_vector_at(
            &execution->interned_enum_descriptors, index
        );
        if (entry != NULL && mmay_bytecode_symbol_equal(entry->symbol, symbol)) {
            return entry;
        }
    }
    return NULL;
}

static const MarimayManagedRecordDescriptor *mmay_bytecode_record_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeRecordId record
) {
    const MarimayBytecodeRecordLayout *layout = mmay_bytecode_record_layout(
        function, record
    );
    MarimayBytecodeRecordDescriptorCacheEntry *existing = NULL;
    MarimayBytecodeRecordDescriptorCacheEntry entry;
    MarimayManagedRecordDescriptor *descriptor = NULL;
    MarimayManagedFieldDescriptor *fields = NULL;
    size_t record_cache_start = 0;
    size_t enum_cache_start = 0;
    size_t array_cache_start = 0;
    size_t entry_index = 0;
    size_t field_bytes = 0;
    size_t field_index = 0;
    if (execution == NULL || layout == NULL || !mmay_size_multiply(
            layout->field_count, sizeof(*fields), &field_bytes
        )) {
        return NULL;
    }
    existing = mmay_bytecode_find_record_descriptor(execution, layout->symbol);
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor
            : NULL;
    }
    record_cache_start = execution->interned_record_descriptors.length;
    enum_cache_start = execution->interned_enum_descriptors.length;
    array_cache_start = execution->array_descriptors.length;
    descriptor = mmay_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    if (field_bytes != 0) {
        fields = mmay_allocate(&execution->allocator, field_bytes);
        if (fields == NULL) {
            mmay_deallocate(
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
    if (!mmay_vector_push(&execution->interned_record_descriptors, &entry)) {
        mmay_deallocate(&execution->allocator, fields, field_bytes);
        mmay_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    entry_index = execution->interned_record_descriptors.length - 1;
    for (field_index = 0; field_index < layout->field_count;
        field_index += 1) {
        const MarimayBytecodeRecordField *field = mmay_bytecode_record_field(
            function, layout, field_index
        );
        if (field == NULL || !mmay_bytecode_managed_field_descriptor(
                execution, function, field->type, field->layout,
                field->is_mutable, &fields[field_index]
            )) {
            mmay_bytecode_nominal_cache_rollback(
                execution, record_cache_start, enum_cache_start,
                array_cache_start
            );
            return NULL;
        }
    }
    existing = mmay_vector_at(
        &execution->interned_record_descriptors, entry_index
    );
    if (existing == NULL) {
        mmay_bytecode_nominal_cache_rollback(
            execution, record_cache_start, enum_cache_start,
            array_cache_start
        );
        return NULL;
    }
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

static const MarimayManagedEnumDescriptor *mmay_bytecode_enum_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEnumId enum_id
) {
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, enum_id
    );
    MarimayBytecodeEnumDescriptorCacheEntry *existing = NULL;
    MarimayBytecodeEnumDescriptorCacheEntry entry;
    MarimayManagedEnumDescriptor *descriptor = NULL;
    MarimayManagedEnumVariantDescriptor *variants = NULL;
    MarimayManagedFieldDescriptor *payloads = NULL;
    size_t record_cache_start = 0;
    size_t enum_cache_start = 0;
    size_t array_cache_start = 0;
    size_t entry_index = 0;
    size_t variant_bytes = 0;
    size_t payload_bytes = 0;
    size_t variant_index = 0;
    size_t payload_index = 0;
    if (execution == NULL || layout == NULL || !mmay_size_multiply(
            layout->variant_count, sizeof(*variants), &variant_bytes
        ) || !mmay_size_multiply(
            layout->payload_count, sizeof(*payloads), &payload_bytes
        )) {
        return NULL;
    }
    existing = mmay_bytecode_find_enum_descriptor(execution, layout->symbol);
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor
            : NULL;
    }
    record_cache_start = execution->interned_record_descriptors.length;
    enum_cache_start = execution->interned_enum_descriptors.length;
    array_cache_start = execution->array_descriptors.length;
    descriptor = mmay_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    if (variant_bytes != 0) {
        variants = mmay_allocate(&execution->allocator, variant_bytes);
    }
    if (payload_bytes != 0) {
        payloads = mmay_allocate(&execution->allocator, payload_bytes);
    }
    if ((variant_bytes != 0 && variants == NULL)
        || (payload_bytes != 0 && payloads == NULL)) {
        mmay_deallocate(&execution->allocator, payloads, payload_bytes);
        mmay_deallocate(&execution->allocator, variants, variant_bytes);
        mmay_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
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
    if (!mmay_vector_push(&execution->interned_enum_descriptors, &entry)) {
        mmay_deallocate(&execution->allocator, payloads, payload_bytes);
        mmay_deallocate(&execution->allocator, variants, variant_bytes);
        mmay_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    entry_index = execution->interned_enum_descriptors.length - 1;
    for (payload_index = 0; payload_index < layout->payload_count;
        payload_index += 1) {
        const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
            &function->enum_payloads, layout->payload_start + payload_index
        );
        if (payload == NULL || !mmay_bytecode_managed_field_descriptor(
                execution, function, payload->type, payload->layout, false,
                &payloads[payload_index]
            )) {
            mmay_bytecode_nominal_cache_rollback(
                execution, record_cache_start, enum_cache_start,
                array_cache_start
            );
            return NULL;
        }
    }
    for (variant_index = 0; variant_index < layout->variant_count;
        variant_index += 1) {
        const MarimayBytecodeEnumVariant *variant = mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + variant_index
        );
        if (variant == NULL || variant->payload_start > layout->payload_count
            || variant->payload_count
                > layout->payload_count - variant->payload_start) {
            mmay_bytecode_nominal_cache_rollback(
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
    existing = mmay_vector_at(
        &execution->interned_enum_descriptors, entry_index
    );
    if (existing == NULL) {
        mmay_bytecode_nominal_cache_rollback(
            execution, record_cache_start, enum_cache_start,
            array_cache_start
        );
        return NULL;
    }
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

MarimayBytecodeRunStatus mmay_bytecode_run_nominal_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout,
    const void **descriptor
) {
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (execution == NULL || function == NULL || descriptor == NULL
        || (type != MMAY_BYTECODE_TYPE_RECORD
            && type != MMAY_BYTECODE_TYPE_ENUM)
        || layout == MMAY_BYTECODE_NO_LAYOUT
        || (type == MMAY_BYTECODE_TYPE_RECORD
            && mmay_bytecode_record_layout(function, layout) == NULL)
        || (type == MMAY_BYTECODE_TYPE_ENUM
            && mmay_bytecode_enum_layout(function, layout) == NULL)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    *descriptor = mmay_bytecode_nominal_reference_descriptor(
        execution, function, type, layout
    );
    return *descriptor != NULL
        ? MMAY_BYTECODE_RUN_OK
        : MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
}

MarimayBytecodeRunStatus mmay_bytecode_run_nominal_value_validate(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value,
    MarimayBytecodeType type,
    uint32_t layout
) {
    const void *expected = NULL;
    const void *actual = NULL;
    size_t variant = 0;
    MarimayBytecodeRunStatus status = mmay_bytecode_run_nominal_descriptor(
        execution, function, type, layout, &expected
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    if (value.type != type) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (type == MMAY_BYTECODE_TYPE_RECORD) {
        const MarimayManagedRecordDescriptor *record = NULL;
        if (mmay_managed_record_describe(
                &execution->managed_heap,
                value.as.managed_reference,
                &record
            ) == MMAY_MANAGED_OK) {
            actual = record;
        }
    } else {
        const MarimayManagedEnumDescriptor *enumeration = NULL;
        if (mmay_managed_enum_describe(
                &execution->managed_heap,
                value.as.managed_reference,
                &enumeration,
                &variant
            ) == MMAY_MANAGED_OK) {
            actual = enumeration;
        }
    }
    return actual == expected
        ? MMAY_BYTECODE_RUN_OK
        : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
}

MarimayBytecodeRunStatus mmay_bytecode_value_to_managed_cell(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeValue *value,
    MarimayManagedCell *cell
) {
    memset(cell, 0, sizeof(*cell));
    if (mmay_bytecode_type_is_integer(value->type)
        || value->type == MMAY_BYTECODE_TYPE_RUNE) {
        cell->f64_bits = value->as.integer_bits;
    } else if (value->type == MMAY_BYTECODE_TYPE_BOOL) {
        cell->f64_bits = value->as.boolean ? 1 : 0;
    } else if (value->type == MMAY_BYTECODE_TYPE_F64) {
        cell->f64_bits = value->as.f64_bits;
    } else if (value->type == MMAY_BYTECODE_TYPE_F32) {
        cell->f64_bits = value->as.integer_bits;
    } else if (value->type == MMAY_BYTECODE_TYPE_STRING) {
        const unsigned char *bytes = NULL;
        size_t length = 0;
        MarimayManagedStatus status;
        if (!mmay_bytecode_execution_string_bytes(
                execution, *value, &bytes, &length
            )) {
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        }
        status = mmay_managed_string_new(
            &execution->managed_heap, bytes, length, &cell->reference
        );
        return mmay_bytecode_run_managed_status(status);
    } else {
        cell->reference = value->as.managed_reference;
    }
    return MMAY_BYTECODE_RUN_OK;
}

MarimayBytecodeRunStatus mmay_bytecode_managed_cell_to_value(
    MarimayBytecodeExecution *execution,
    MarimayManagedCell cell,
    MarimayBytecodeValue *value
) {
    if (mmay_bytecode_type_is_integer(value->type)
        || value->type == MMAY_BYTECODE_TYPE_RUNE) {
        value->as.integer_bits = cell.f64_bits;
    } else if (value->type == MMAY_BYTECODE_TYPE_BOOL) {
        value->as.boolean = cell.f64_bits != 0;
    } else if (value->type == MMAY_BYTECODE_TYPE_F64) {
        value->as.f64_bits = cell.f64_bits;
    } else if (value->type == MMAY_BYTECODE_TYPE_F32) {
        value->as.integer_bits = cell.f64_bits;
    } else if (value->type == MMAY_BYTECODE_TYPE_STRING) {
        const unsigned char *bytes = NULL;
        size_t length = 0;
        if (mmay_managed_string_bytes(
                &execution->managed_heap, cell.reference, &bytes, &length
            ) != MMAY_MANAGED_OK) {
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        }
        return mmay_bytecode_execution_intern(
            execution, bytes, length, &value->as.reference
        );
    } else {
        value->as.managed_reference = cell.reference;
    }
    return MMAY_BYTECODE_RUN_OK;
}

MarimayBytecodeRunStatus mmay_bytecode_execution_new_numeric_option(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    uint32_t enum_id,
    bool some,
    MarimayBytecodeValue payload,
    MarimayBytecodeValue *target
) {
    const MarimayManagedEnumDescriptor *descriptor = execution != NULL
        ? mmay_bytecode_enum_descriptor(execution, function, enum_id)
        : NULL;
    MarimayManagedCell cell;
    MarimayBytecodeRunStatus status;
    if (execution == NULL || descriptor == NULL || target == NULL) {
        return execution == NULL
            ? MMAY_BYTECODE_RUN_INVALID_ARGUMENT
            : MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(&cell, 0, sizeof(cell));
    if (some) {
        status = mmay_bytecode_value_to_managed_cell(
            execution, &payload, &cell
        );
        if (status != MMAY_BYTECODE_RUN_OK) {
            return status;
        }
    }
    return mmay_bytecode_run_managed_status(mmay_managed_enum_new(
        &execution->managed_heap,
        descriptor,
        some ? 0 : 1,
        some ? &cell : NULL,
        some ? 1 : 0,
        &target->as.managed_reference
    ));
}

static void mmay_bytecode_temporary_cells_dispose(
    MarimayBytecodeTemporaryCells *temporary,
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    if (temporary->rooted) {
        (void)mmay_managed_root_pop(
            &execution->managed_heap, &temporary->roots
        );
    }
    mmay_deallocate(
        &allocator, temporary->locations, temporary->location_bytes
    );
    mmay_deallocate(&allocator, temporary->cells, temporary->cell_bytes);
    memset(temporary, 0, sizeof(*temporary));
}

static MarimayBytecodeRunStatus mmay_bytecode_prepare_temporary_cells(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue *registers,
    bool is_record,
    size_t argument_start,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayBytecodeTemporaryCells *temporary
) {
    size_t index = 0;
    size_t string_count = 0;
    size_t string_index = 0;
    MarimayManagedStatus root_status;
    memset(temporary, 0, sizeof(*temporary));
    if (!mmay_size_multiply(
            argument_count, sizeof(*temporary->cells), &temporary->cell_bytes
        )) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayBytecodeType type = is_record
            ? ((const MarimayBytecodeRecordArgument *)mmay_vector_at_const(
                &function->record_arguments, argument_start + index
            ))->type
            : ((const MarimayBytecodeEnumArgument *)mmay_vector_at_const(
                &function->enum_arguments, argument_start + index
            ))->type;
        if (type == MMAY_BYTECODE_TYPE_STRING) {
            string_count += 1;
        }
    }
    if (!mmay_size_multiply(
            string_count,
            sizeof(*temporary->locations),
            &temporary->location_bytes
        )) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (temporary->cell_bytes != 0) {
        temporary->cells = mmay_allocate(
            &allocator, temporary->cell_bytes
        );
    }
    if (temporary->location_bytes != 0) {
        temporary->locations = mmay_allocate(
            &allocator, temporary->location_bytes
        );
    }
    if ((temporary->cell_bytes != 0 && temporary->cells == NULL)
        || (temporary->location_bytes != 0
            && temporary->locations == NULL)) {
        mmay_bytecode_temporary_cells_dispose(
            temporary, execution, allocator
        );
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (temporary->cell_bytes != 0) {
        memset(temporary->cells, 0, temporary->cell_bytes);
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayBytecodeType type = is_record
            ? ((const MarimayBytecodeRecordArgument *)mmay_vector_at_const(
                &function->record_arguments, argument_start + index
            ))->type
            : ((const MarimayBytecodeEnumArgument *)mmay_vector_at_const(
                &function->enum_arguments, argument_start + index
            ))->type;
        if (type == MMAY_BYTECODE_TYPE_STRING) {
            temporary->cells[index].reference =
                mmay_managed_invalid_reference();
            temporary->locations[string_index] =
                &temporary->cells[index].reference;
            string_index += 1;
        }
    }
    root_status = mmay_managed_root_push(
        &execution->managed_heap,
        &temporary->roots,
        temporary->locations,
        string_count
    );
    if (root_status != MMAY_MANAGED_OK) {
        mmay_bytecode_temporary_cells_dispose(
            temporary, execution, allocator
        );
        return mmay_bytecode_run_managed_status(root_status);
    }
    temporary->rooted = true;
    for (index = 0; index < argument_count; index += 1) {
        uint32_t register_id = is_record
            ? ((const MarimayBytecodeRecordArgument *)mmay_vector_at_const(
                &function->record_arguments, argument_start + index
            ))->register_id
            : ((const MarimayBytecodeEnumArgument *)mmay_vector_at_const(
                &function->enum_arguments, argument_start + index
            ))->register_id;
        MarimayBytecodeRunStatus status = mmay_bytecode_value_to_managed_cell(
            execution, &registers[register_id], &temporary->cells[index]
        );
        if (status != MMAY_BYTECODE_RUN_OK) {
            mmay_bytecode_temporary_cells_dispose(
                temporary, execution, allocator
            );
            return status;
        }
    }
    return MMAY_BYTECODE_RUN_OK;
}

MarimayBytecodeRunStatus mmay_bytecode_run_nominal_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    if (execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_RECORD_NEW: {
            const MarimayBytecodeRecordConstruction *construction =
                mmay_bytecode_record_construction(
                    function,
                    (MarimayBytecodeRecordConstructionId)
                        instruction->operands[1].unsigned_value
                );
            const MarimayManagedRecordDescriptor *descriptor = construction != NULL
                ? mmay_bytecode_record_descriptor(
                    execution, function, construction->record
                )
                : NULL;
            MarimayBytecodeTemporaryCells temporary;
            MarimayManagedStatus managed_status;
            MarimayBytecodeRunStatus status;
            if (construction == NULL || descriptor == NULL) {
                return construction == NULL
                    ? MMAY_BYTECODE_RUN_INVALID_BYTECODE
                    : MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            status = mmay_bytecode_prepare_temporary_cells(
                execution, function, registers, true,
                construction->argument_start, construction->argument_count,
                allocator, &temporary
            );
            if (status != MMAY_BYTECODE_RUN_OK) {
                return status;
            }
            managed_status = mmay_managed_record_new(
                &execution->managed_heap,
                descriptor,
                temporary.cells,
                construction->argument_count,
                &registers[target].as.managed_reference
            );
            mmay_bytecode_temporary_cells_dispose(
                &temporary, execution, allocator
            );
            return mmay_bytecode_run_managed_status(managed_status);
        }
        case MMAY_OPCODE_RECORD_GET: {
            MarimayManagedCell cell;
            MarimayManagedStatus managed_status = mmay_managed_record_get(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                &cell
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_run_managed_status(managed_status);
            }
            return mmay_bytecode_managed_cell_to_value(
                execution, cell, &registers[target]
            );
        }
        case MMAY_OPCODE_RECORD_SET: {
            MarimayManagedCell cell;
            MarimayBytecodeRunStatus status = mmay_bytecode_value_to_managed_cell(
                execution, &registers[target], &cell
            );
            if (status != MMAY_BYTECODE_RUN_OK) {
                return status;
            }
            return mmay_bytecode_run_managed_status(mmay_managed_record_set(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                cell
            ));
        }
        case MMAY_OPCODE_RECORD_IDENTITY_SAME:
            registers[target].as.boolean = mmay_managed_reference_equal(
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                registers[(size_t)instruction->operands[2].unsigned_value]
                    .as.managed_reference
            );
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_OPCODE_ENUM_NEW: {
            const MarimayBytecodeEnumConstruction *construction =
                mmay_bytecode_enum_construction(
                    function,
                    (MarimayBytecodeEnumConstructionId)
                        instruction->operands[1].unsigned_value
                );
            const MarimayManagedEnumDescriptor *descriptor = construction != NULL
                ? mmay_bytecode_enum_descriptor(
                    execution, function, construction->enum_id
                )
                : NULL;
            MarimayBytecodeTemporaryCells temporary;
            MarimayManagedStatus managed_status;
            MarimayBytecodeRunStatus status;
            if (construction == NULL || descriptor == NULL) {
                return construction == NULL
                    ? MMAY_BYTECODE_RUN_INVALID_BYTECODE
                    : MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            status = mmay_bytecode_prepare_temporary_cells(
                execution, function, registers, false,
                construction->argument_start, construction->argument_count,
                allocator, &temporary
            );
            if (status != MMAY_BYTECODE_RUN_OK) {
                return status;
            }
            managed_status = mmay_managed_enum_new(
                &execution->managed_heap,
                descriptor,
                construction->variant,
                temporary.cells,
                construction->argument_count,
                &registers[target].as.managed_reference
            );
            mmay_bytecode_temporary_cells_dispose(
                &temporary, execution, allocator
            );
            return mmay_bytecode_run_managed_status(managed_status);
        }
        case MMAY_OPCODE_ENUM_IS_VARIANT: {
            bool matches = false;
            MarimayManagedStatus managed_status = mmay_managed_enum_is_variant(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                &matches
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_run_managed_status(managed_status);
            }
            registers[target].as.boolean = matches;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_ENUM_REQUIRE_VARIANT: {
            bool matches = false;
            size_t input = (size_t)instruction->operands[1].unsigned_value;
            MarimayManagedStatus managed_status = mmay_managed_enum_is_variant(
                &execution->managed_heap,
                registers[input].as.managed_reference,
                (size_t)instruction->operands[2].signed_value,
                &matches
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_run_managed_status(managed_status);
            }
            if (!matches) {
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            registers[target] = registers[input];
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_ENUM_GET_PAYLOAD: {
            uint64_t selector =
                (uint64_t)instruction->operands[2].signed_value;
            MarimayManagedCell cell;
            MarimayManagedStatus managed_status = mmay_managed_enum_get_payload(
                &execution->managed_heap,
                registers[(size_t)instruction->operands[1].unsigned_value]
                    .as.managed_reference,
                (size_t)(selector >> 32),
                (size_t)(selector & UINT32_MAX),
                &cell
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_run_managed_status(managed_status);
            }
            return mmay_bytecode_managed_cell_to_value(
                execution, cell, &registers[target]
            );
        }
        default:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
}
