#include "vm_internal.h"

#include <string.h>

#include "program_internal.h"

typedef struct MarimayBytecodeArrayDescriptorEntry {
    const MarimayBytecodeFunction *function;
    uint32_t layout;
    MarimayManagedArrayDescriptor *descriptor;
    bool building;
    bool complete;
} MarimayBytecodeArrayDescriptorEntry;

bool mmay_bytecode_array_cache_init(
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    return execution != NULL && mmay_vector_init(
        &execution->array_descriptors,
        allocator,
        sizeof(MarimayBytecodeArrayDescriptorEntry)
    );
}

void mmay_bytecode_array_cache_rollback(
    MarimayBytecodeExecution *execution,
    size_t length
) {
    if (execution == NULL) {
        return;
    }
    while (execution->array_descriptors.length > length) {
        size_t index = execution->array_descriptors.length - 1;
        MarimayBytecodeArrayDescriptorEntry *entry = mmay_vector_at(
            &execution->array_descriptors, index
        );
        if (entry != NULL) {
            mmay_deallocate(
                &execution->allocator,
                entry->descriptor,
                entry->descriptor == NULL ? 0 : sizeof(*entry->descriptor)
            );
            memset(entry, 0, sizeof(*entry));
        }
        execution->array_descriptors.length = index;
    }
}

void mmay_bytecode_array_cache_dispose(MarimayBytecodeExecution *execution) {
    if (execution == NULL) {
        return;
    }
    mmay_bytecode_array_cache_rollback(execution, 0);
    mmay_vector_dispose(&execution->array_descriptors);
}

const MarimayManagedArrayDescriptor *mmay_bytecode_array_descriptor(
    MarimayBytecodeType type
) {
    if (type == MMAY_BYTECODE_TYPE_ARRAY_F64) {
        return &mmay_managed_array_f64_descriptor;
    }
    if (type == MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64) {
        return &mmay_managed_array_array_f64_descriptor;
    }
    return NULL;
}

bool mmay_bytecode_managed_value_valid(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value
) {
    const MarimayManagedArrayDescriptor *actual = NULL;
    const MarimayManagedArrayDescriptor *expected =
        mmay_bytecode_array_descriptor(value.type);
    return execution != NULL && execution->initialized && expected != NULL
        && mmay_managed_array_describe(
            &execution->managed_heap, value.as.managed_reference, &actual
        ) == MMAY_MANAGED_OK && actual == expected;
}

static MarimayBytecodeArrayDescriptorEntry *mmay_bytecode_find_array_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    uint32_t layout
) {
    size_t index = 0;
    for (index = 0; index < execution->array_descriptors.length; index += 1) {
        MarimayBytecodeArrayDescriptorEntry *entry = mmay_vector_at(
            &execution->array_descriptors, index
        );
        if (entry != NULL && mmay_bytecode_program_same_type(
                entry->function,
                MMAY_BYTECODE_TYPE_ARRAY,
                entry->layout,
                function,
                MMAY_BYTECODE_TYPE_ARRAY,
                layout
            )) {
            return entry;
        }
    }
    return NULL;
}

static bool mmay_bytecode_build_array_element_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeArrayType *array_type,
    MarimayManagedArrayDescriptor *descriptor
) {
    MarimayBytecodeType type = array_type->element_type;
    uint32_t layout = array_type->element_layout;
    if (type == MMAY_BYTECODE_TYPE_STRING) {
        descriptor->cell_kind = MMAY_MANAGED_CELL_REFERENCE;
        descriptor->storage_kind = MMAY_MANAGED_ARRAY_STORAGE_CELL;
        descriptor->storage_stride = sizeof(MarimayManagedCell);
        descriptor->reference_kind = MMAY_MANAGED_OBJECT_STRING;
        return true;
    }
    if (mmay_bytecode_type_is_array(type)
        || type == MMAY_BYTECODE_TYPE_RECORD
        || type == MMAY_BYTECODE_TYPE_ENUM
        || type == MMAY_BYTECODE_TYPE_INTERFACE) {
        descriptor->cell_kind = MMAY_MANAGED_CELL_REFERENCE;
        descriptor->storage_kind = MMAY_MANAGED_ARRAY_STORAGE_CELL;
        descriptor->storage_stride = sizeof(MarimayManagedCell);
        descriptor->reference_kind = mmay_bytecode_nominal_object_kind(type);
        descriptor->reference_descriptor =
            mmay_bytecode_nominal_reference_descriptor(
                execution, function, type, layout
            );
        return descriptor->reference_kind != MMAY_MANAGED_OBJECT_INVALID
            && descriptor->reference_descriptor != NULL;
    }
    descriptor->cell_kind = MMAY_MANAGED_CELL_RAW64;
    if (type == MMAY_BYTECODE_TYPE_BOOL || type == MMAY_BYTECODE_TYPE_I8
        || type == MMAY_BYTECODE_TYPE_U8) {
        descriptor->storage_kind = MMAY_MANAGED_ARRAY_STORAGE_RAW8;
        descriptor->storage_stride = sizeof(uint8_t);
    } else if (type == MMAY_BYTECODE_TYPE_I16
        || type == MMAY_BYTECODE_TYPE_U16) {
        descriptor->storage_kind = MMAY_MANAGED_ARRAY_STORAGE_RAW16;
        descriptor->storage_stride = sizeof(uint16_t);
    } else if (type == MMAY_BYTECODE_TYPE_I32
        || type == MMAY_BYTECODE_TYPE_U32
        || type == MMAY_BYTECODE_TYPE_F32) {
        descriptor->storage_kind = MMAY_MANAGED_ARRAY_STORAGE_RAW32;
        descriptor->storage_stride = sizeof(uint32_t);
    } else if (type == MMAY_BYTECODE_TYPE_I64
        || type == MMAY_BYTECODE_TYPE_U64
        || type == MMAY_BYTECODE_TYPE_F64) {
        descriptor->storage_kind = MMAY_MANAGED_ARRAY_STORAGE_RAW64;
        descriptor->storage_stride = sizeof(uint64_t);
    } else {
        return false;
    }
    descriptor->reference_kind = MMAY_MANAGED_OBJECT_INVALID;
    return true;
}

const MarimayManagedArrayDescriptor *mmay_bytecode_array_descriptor_for(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout
) {
    const MarimayBytecodeArrayType *array_type = NULL;
    MarimayBytecodeArrayDescriptorEntry *existing = NULL;
    MarimayBytecodeArrayDescriptorEntry entry;
    MarimayManagedArrayDescriptor *descriptor = NULL;
    size_t cache_start = 0;
    size_t entry_index = 0;
    if (type != MMAY_BYTECODE_TYPE_ARRAY) {
        return mmay_bytecode_array_descriptor(type);
    }
    if (execution == NULL || !execution->initialized || function == NULL) {
        return NULL;
    }
    array_type = mmay_bytecode_array_type(function, layout);
    if (array_type == NULL) {
        return NULL;
    }
    existing = mmay_bytecode_find_array_descriptor(
        execution, function, layout
    );
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor : NULL;
    }
    cache_start = execution->array_descriptors.length;
    descriptor = mmay_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        execution->array_descriptor_allocation_failed = true;
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    memset(&entry, 0, sizeof(entry));
    entry.function = function;
    entry.layout = layout;
    entry.descriptor = descriptor;
    entry.building = true;
    if (!mmay_vector_push(&execution->array_descriptors, &entry)) {
        execution->array_descriptor_allocation_failed = true;
        mmay_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    entry_index = execution->array_descriptors.length - 1;
    if (!mmay_bytecode_build_array_element_descriptor(
            execution, function, array_type, descriptor
        )) {
        execution->array_descriptor_allocation_failed = true;
        mmay_bytecode_array_cache_rollback(execution, cache_start);
        return NULL;
    }
    existing = mmay_vector_at(&execution->array_descriptors, entry_index);
    if (existing == NULL) {
        execution->array_descriptor_allocation_failed = true;
        mmay_bytecode_array_cache_rollback(execution, cache_start);
        return NULL;
    }
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

bool mmay_bytecode_managed_value_valid_for(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value,
    uint32_t layout
) {
    const MarimayManagedArrayDescriptor *actual = NULL;
    const MarimayManagedArrayDescriptor *expected =
        mmay_bytecode_array_descriptor_for(
            execution, function, value.type, layout
        );
    return execution != NULL && execution->initialized && expected != NULL
        && mmay_managed_array_describe(
            &execution->managed_heap, value.as.managed_reference, &actual
        ) == MMAY_MANAGED_OK && actual == expected;
}
